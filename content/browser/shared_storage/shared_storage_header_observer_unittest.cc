// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_storage/shared_storage_header_observer.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "content/browser/navigation_or_document_handle.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/common/content_client.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/shared_storage_test_utils.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_shared_storage_header_observer.h"
#include "content/test/test_web_contents.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/permissions_policy/origin_with_possible_wildcards.h"
#include "services/network/public/cpp/permissions_policy/permissions_policy_declaration.h"
#include "services/network/public/cpp/shared_storage_utils.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom-shared.h"
#include "services/network/public/mojom/url_loader_network_service_observer.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest-param-test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

const char kMainOrigin[] = "https://main.test";
const char kMainOrigin1[] = "https://main1.test";
const char kMainOrigin2[] = "https://main2.test";
const char kChildOrigin1[] = "https://child1.test";
const char kChildOrigin2[] = "https://child2.test";
const char kTestOrigin1[] = "https://a.test";
const char kTestOrigin2[] = "https://b.test";
const char kTestOrigin3[] = "https://c.test";

using MethodWithOptionsPtr =
    network::mojom::SharedStorageModifierMethodWithOptionsPtr;
using OperationResult = storage::SharedStorageManager::OperationResult;
using GetResult = storage::SharedStorageManager::GetResult;
using ContextType = StoragePartitionImpl::ContextType;

enum class TestCaseType {
  kNavigationRequestContextIframePermissionEnabled = 0,
  kNavigationRequestContextIframePermissionDisabled = 1,
  kNavigationRequestContextMainFrame = 2,
  kRenderFrameHostContextIframePermissionEnabled = 3,
  kRenderFrameHostContextIframePermissionDisabled = 4,
  kRenderFrameHostContextIframeArriveBeforeCommit = 5,
  kRenderFrameHostContextMainFramePermissionEnabled = 6,
  kRenderFrameHostContextMainFramePermissionDisabled = 7,
  kRenderFrameHostContextMainFrameArriveBeforeCommit = 8,
  kRenderFrameHostContextNoRenderFrameHost = 9,
};

using OperationAndResult = SharedStorageWriteOperationAndResult;

[[nodiscard]] network::ParsedPermissionsPolicy
MakeSharedStoragePermissionsPolicy(const url::Origin& request_origin,
                                   bool shared_storage_enabled_for_request,
                                   bool shared_storage_enabled_for_all) {
  std::vector<network::OriginWithPossibleWildcards> allowed_origins =
      shared_storage_enabled_for_request
          ? std::vector<network::OriginWithPossibleWildcards>(
                {*network::OriginWithPossibleWildcards::FromOrigin(
                    request_origin)})
          : std::vector<network::OriginWithPossibleWildcards>();
  return network::ParsedPermissionsPolicy(
      {network::ParsedPermissionsPolicyDeclaration(
          network::mojom::PermissionsPolicyFeature::kSharedStorage,
          std::move(allowed_origins),
          /*self_if_matches=*/std::nullopt,
          /*matches_all_origins=*/shared_storage_enabled_for_all,
          /*matches_opaque_src=*/false)});
}

class MockContentBrowserClient : public ContentBrowserClient {
 public:
  bool IsSharedStorageAllowed(
      content::BrowserContext* browser_context,
      content::RenderFrameHost* rfh,
      const url::Origin& top_frame_origin,
      const url::Origin& accessing_origin,
      std::string* out_debug_message,
      bool* out_block_is_site_setting_specific) override {
    if (bypass_shared_storage_allowed_count_ > 0) {
      bypass_shared_storage_allowed_count_--;
      return true;
    }

    return ContentBrowserClient::IsSharedStorageAllowed(
        browser_context, rfh, top_frame_origin, accessing_origin,
        out_debug_message, out_block_is_site_setting_specific);
  }

  void set_bypass_shared_storage_allowed_count(int count) {
    CHECK_EQ(bypass_shared_storage_allowed_count_, 0);
    CHECK_GE(count, 0);
    bypass_shared_storage_allowed_count_ = count;
  }

 private:
  int bypass_shared_storage_allowed_count_ = 0;
};

class SharedStorageHeaderObserverTest
    : public RenderViewHostTestHarness,
      public testing::WithParamInterface<TestCaseType> {
 public:
  SharedStorageHeaderObserverTest() {
    feature_list_.InitAndEnableFeature(network::features::kSharedStorageAPI);
  }

  ~SharedStorageHeaderObserverTest() override = default;

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    mock_content_browser_client_ = std::make_unique<MockContentBrowserClient>();
    old_content_browser_client_ =
        content::SetBrowserClientForTesting(mock_content_browser_client_.get());
    observer_ = std::make_unique<TestSharedStorageHeaderObserver>(
        browser_context()->GetDefaultStoragePartition());
    NavigateMainFrameAndGetHandle(GURL(kMainOrigin),
                                  url::Origin::Create(GURL(kMainOrigin)),
                                  /*shared_storage_enabled_for_request=*/true,
                                  /*shared_storage_enabled_for_all=*/true);

    // Intentionally use duplicates to test the cases where the previously used
    // URL is the same as well as where it is different.
    main_urls_ = {kMainOrigin1, kMainOrigin2, kMainOrigin2};
    child_urls_ = {kChildOrigin1, kChildOrigin1, kChildOrigin2};
  }

  void TearDown() override {
    // `RenderViewHostTestHarness::TearDown()` leads to the destruction of
    // `TestBrowserContext` which leads to
    // `BrowserContextImpl::ShutdownStoragePartitions()` being called. This
    // destroys the `StoragePartitionImpl` that
    // `SharedStorageHeaderObserver::storage_partition_` points to.
    // Destroy the observer before `RenderViewHostTestHarness::TearDown()` to
    // avoid it holding an Across Tasks Dangling Ptr to the
    // `StoragePartitionImpl`.
    observer_.reset();
    content::SetBrowserClientForTesting(old_content_browser_client_);
    old_content_browser_client_ = nullptr;
    RenderViewHostTestHarness::TearDown();
  }

  void set_bypass_shared_storage_allowed_count(int count) {
    mock_content_browser_client_->set_bypass_shared_storage_allowed_count(
        count);
  }

  bool ExpectSuccess() const {
    return GetParam() ==
               TestCaseType::kNavigationRequestContextIframePermissionEnabled ||
           GetParam() ==
               TestCaseType::kRenderFrameHostContextIframePermissionEnabled ||
           GetParam() ==
               TestCaseType::kRenderFrameHostContextIframeArriveBeforeCommit ||
           GetParam() ==
               TestCaseType::
                   kRenderFrameHostContextMainFramePermissionEnabled ||
           GetParam() ==
               TestCaseType::kRenderFrameHostContextMainFrameArriveBeforeCommit;
  }

  ContextType GetContextType() {
    switch (GetParam()) {
      case TestCaseType::kNavigationRequestContextIframePermissionEnabled:
        [[fallthrough]];
      case TestCaseType::kNavigationRequestContextIframePermissionDisabled:
        [[fallthrough]];
      case TestCaseType::kNavigationRequestContextMainFrame:
        return ContextType::kNavigationRequestContext;
      case TestCaseType::kRenderFrameHostContextIframePermissionEnabled:
        [[fallthrough]];
      case TestCaseType::kRenderFrameHostContextIframePermissionDisabled:
        [[fallthrough]];
      case TestCaseType::kRenderFrameHostContextIframeArriveBeforeCommit:
        [[fallthrough]];
      case TestCaseType::kRenderFrameHostContextMainFramePermissionEnabled:
        [[fallthrough]];
      case TestCaseType::kRenderFrameHostContextMainFramePermissionDisabled:
        [[fallthrough]];
      case TestCaseType::kRenderFrameHostContextMainFrameArriveBeforeCommit:
        [[fallthrough]];
      case TestCaseType::kRenderFrameHostContextNoRenderFrameHost:
        return ContextType::kRenderFrameHostContext;
      default:
        NOTREACHED();
    }
  }

  GURL NextMainURL() {
    size_t next_index = main_url_index_++;
    main_url_index_ %= main_urls_.size();
    return GURL(main_urls_[next_index]);
  }

  GURL NextChildURL() {
    size_t next_index = child_url_index_++;
    child_url_index_ %= child_urls_.size();
    return GURL(child_urls_[next_index]);
  }

  NavigationOrDocumentHandle* GetNavigationOrDocumentHandle(
      const url::Origin& request_origin) {
    switch (GetParam()) {
      case TestCaseType::kNavigationRequestContextIframePermissionEnabled:
        // Renavigate main frame to ensure that iframe's parent policy is set as
        // needed.
        NavigateMainFrameAndGetHandle(
            NextMainURL(), request_origin,
            /*shared_storage_enabled_for_request=*/true,
            /*shared_storage_enabled_for_all=*/false);
        return CreateIframeNavigationRequestAndGetHandle(
            NextChildURL(), request_origin,
            /*shared_storage_enabled_for_request=*/true,
            /*shared_storage_enabled_for_all=*/false);
      case TestCaseType::kNavigationRequestContextIframePermissionDisabled:
        // Renavigate main frame to ensure that iframe's parent policy is set as
        // needed.
        NavigateMainFrameAndGetHandle(
            NextMainURL(), request_origin,
            /*shared_storage_enabled_for_request=*/false,
            /*shared_storage_enabled_for_all=*/false);
        return CreateIframeNavigationRequestAndGetHandle(
            NextChildURL(), request_origin,
            /*shared_storage_enabled_for_request=*/false,
            /*shared_storage_enabled_for_all=*/false);
      case TestCaseType::kNavigationRequestContextMainFrame:
        return CreateMainFrameNavigationRequestAndGetHandle(
            NextMainURL(), request_origin,
            /*shared_storage_enabled_for_request=*/true,
            /*shared_storage_enabled_for_all=*/false);
      case TestCaseType::kRenderFrameHostContextIframePermissionEnabled:
        return CreateAndNavigateIframeAndGetHandle(
            NextChildURL(), request_origin,
            /*shared_storage_enabled_for_request=*/true,
            /*shared_storage_enabled_for_all=*/false);
      case TestCaseType::kRenderFrameHostContextIframePermissionDisabled:
        return CreateAndNavigateIframeAndGetHandle(
            NextChildURL(), request_origin,
            /*shared_storage_enabled_for_request=*/false,
            /*shared_storage_enabled_for_all=*/false);
      case TestCaseType::kRenderFrameHostContextIframeArriveBeforeCommit:
        return CreateAndGetIframeReadyToCommitAndGetHandle(
            NextChildURL(), request_origin,
            /*shared_storage_enabled_for_request=*/true,
            /*shared_storage_enabled_for_all=*/false);
      case TestCaseType::kRenderFrameHostContextMainFramePermissionEnabled:
        return NavigateMainFrameAndGetHandle(
            NextMainURL(), request_origin,
            /*shared_storage_enabled_for_request=*/true,
            /*shared_storage_enabled_for_all=*/false);
      case TestCaseType::kRenderFrameHostContextMainFramePermissionDisabled:
        return NavigateMainFrameAndGetHandle(
            NextMainURL(), request_origin,
            /*shared_storage_enabled_for_request=*/false,
            /*shared_storage_enabled_for_all=*/false);
      case TestCaseType::kRenderFrameHostContextMainFrameArriveBeforeCommit:
        return GetMainFrameReadyToCommitAndGetHandle(
            NextMainURL(), request_origin,
            /*shared_storage_enabled_for_request=*/true,
            /*shared_storage_enabled_for_all=*/true);
      case TestCaseType::kRenderFrameHostContextNoRenderFrameHost:
        if (!handle_for_null_rfh_) {
          handle_for_null_rfh_ = NavigationOrDocumentHandle::CreateForDocument(
              GlobalRenderFrameHostId());
        }
        return handle_for_null_rfh_.get();
      default:
        NOTREACHED();
    }
  }

  void SetUpMainFrameNavigation(const GURL& url,
                                const url::Origin& request_origin,
                                bool shared_storage_enabled_for_request,
                                bool shared_storage_enabled_for_all) {
    main_frame_navigation_simulator_ =
        content::NavigationSimulator::CreateBrowserInitiated(url,
                                                             web_contents());
    main_frame_navigation_simulator_->SetPermissionsPolicyHeader(
        MakeSharedStoragePermissionsPolicy(request_origin,
                                           shared_storage_enabled_for_request,
                                           shared_storage_enabled_for_all));
    main_frame_navigation_simulator_->Start();
  }

  NavigationOrDocumentHandle* CreateMainFrameNavigationRequestAndGetHandle(
      const GURL& url,
      const url::Origin& request_origin,
      bool shared_storage_enabled_for_request,
      bool shared_storage_enabled_for_all) {
    SetUpMainFrameNavigation(url, request_origin,
                             shared_storage_enabled_for_request,
                             shared_storage_enabled_for_all);
    return NavigationRequest::From(
               main_frame_navigation_simulator_->GetNavigationHandle())
        ->navigation_or_document_handle()
        .get();
  }

  NavigationOrDocumentHandle* NavigateMainFrameAndGetHandle(
      const GURL& url,
      const url::Origin& request_origin,
      bool shared_storage_enabled_for_request,
      bool shared_storage_enabled_for_all) {
    SetUpMainFrameNavigation(url, request_origin,
                             shared_storage_enabled_for_request,
                             shared_storage_enabled_for_all);
    main_frame_navigation_simulator_->Commit();
    return static_cast<RenderFrameHostImpl*>(
               main_frame_navigation_simulator_->GetFinalRenderFrameHost())
        ->GetNavigationOrDocumentHandle()
        .get();
  }

  NavigationOrDocumentHandle* GetMainFrameReadyToCommitAndGetHandle(
      const GURL& url,
      const url::Origin& request_origin,
      bool shared_storage_enabled_for_request,
      bool shared_storage_enabled_for_all) {
    SetUpMainFrameNavigation(url, request_origin,
                             shared_storage_enabled_for_request,
                             shared_storage_enabled_for_all);
    main_frame_navigation_simulator_->ReadyToCommit();
    return static_cast<RenderFrameHostImpl*>(
               main_frame_navigation_simulator_->GetFinalRenderFrameHost())
        ->GetNavigationOrDocumentHandle()
        .get();
  }

  void SetUpIframeNavigation(const GURL& url,
                             const url::Origin& request_origin,
                             bool shared_storage_enabled_for_request,
                             bool shared_storage_enabled_for_all) {
    content::RenderFrameHost* child_rfh =
        content::RenderFrameHostTester::For(main_rfh())
            ->AppendChild("myiframe");
    iframe_navigation_simulator_ =
        content::NavigationSimulator::CreateRendererInitiated(url, child_rfh);
    iframe_navigation_simulator_->SetTransition(
        ui::PAGE_TRANSITION_MANUAL_SUBFRAME);
    iframe_navigation_simulator_->Start();
    iframe_navigation_simulator_->SetPermissionsPolicyHeader(
        MakeSharedStoragePermissionsPolicy(request_origin,
                                           shared_storage_enabled_for_request,
                                           shared_storage_enabled_for_all));
  }

  NavigationOrDocumentHandle* CreateIframeNavigationRequestAndGetHandle(
      const GURL& url,
      const url::Origin& request_origin,
      bool shared_storage_enabled_for_request,
      bool shared_storage_enabled_for_all) {
    SetUpIframeNavigation(url, request_origin,
                          shared_storage_enabled_for_request,
                          shared_storage_enabled_for_all);
    return NavigationRequest::From(
               iframe_navigation_simulator_->GetNavigationHandle())
        ->navigation_or_document_handle()
        .get();
  }

  NavigationOrDocumentHandle* CreateAndNavigateIframeAndGetHandle(
      const GURL& url,
      const url::Origin& request_origin,
      bool shared_storage_enabled_for_request,
      bool shared_storage_enabled_for_all) {
    SetUpIframeNavigation(url, request_origin,
                          shared_storage_enabled_for_request,
                          shared_storage_enabled_for_all);
    iframe_navigation_simulator_->Commit();
    return static_cast<RenderFrameHostImpl*>(
               iframe_navigation_simulator_->GetFinalRenderFrameHost())
        ->GetNavigationOrDocumentHandle()
        .get();
  }

  NavigationOrDocumentHandle* CreateAndGetIframeReadyToCommitAndGetHandle(
      const GURL& url,
      const url::Origin& request_origin,
      bool shared_storage_enabled_for_request,
      bool shared_storage_enabled_for_all) {
    SetUpIframeNavigation(url, request_origin,
                          shared_storage_enabled_for_request,
                          shared_storage_enabled_for_all);
    iframe_navigation_simulator_->ReadyToCommit();
    return static_cast<RenderFrameHostImpl*>(
               iframe_navigation_simulator_->GetFinalRenderFrameHost())
        ->GetNavigationOrDocumentHandle()
        .get();
  }

  void RunHeaderReceived(const url::Origin& request_origin,
                         std::vector<MethodWithOptionsPtr> methods_with_options,
                         const std::optional<std::string>& with_lock) {
    base::RunLoop loop;
    base::OnceCallback<void(std::string_view error)> bad_message_callback =
        base::BindLambdaForTesting([&](std::string_view error) {
          LOG(ERROR) << error;
          std::move(loop.QuitClosure()).Run();
        });
    auto* navigation_or_document_handle =
        GetNavigationOrDocumentHandle(request_origin);
    observer_->HeaderReceived(
        request_origin, GetContextType(), navigation_or_document_handle,
        CloneSharedStorageMethods(methods_with_options), with_lock,
        loop.QuitClosure(), std::move(bad_message_callback),
        /*can_defer=*/true);
    loop.Run();

    if (GetContextType() == ContextType::kRenderFrameHostContext &&
        navigation_or_document_handle->GetDocument() &&
        navigation_or_document_handle->GetDocument()->IsInLifecycleState(
            RenderFrameHost::LifecycleState::kPendingCommit)) {
      base::RunLoop commit_loop;
      base::OnceCallback<void(NavigationOrDocumentHandle*)> commit_callback =
          base::BindLambdaForTesting([&](NavigationOrDocumentHandle* handle) {
            std::move(commit_loop.QuitClosure()).Run();
          });
      static_cast<RenderFrameHostImpl*>(
          navigation_or_document_handle->GetDocument())
          ->AddDeferredSharedStorageHeaderCallback(std::move(commit_callback));
      switch (GetParam()) {
        case TestCaseType::kRenderFrameHostContextIframeArriveBeforeCommit:
          ASSERT_TRUE(iframe_navigation_simulator_);
          iframe_navigation_simulator_->Commit();
          break;
        case TestCaseType::kRenderFrameHostContextMainFrameArriveBeforeCommit:
          ASSERT_TRUE(main_frame_navigation_simulator_);
          main_frame_navigation_simulator_->Commit();
          break;
        default:
          LOG(ERROR)
              << "Encountered unexpectedly deferred shared storage methods";
          NOTREACHED();
      }
      commit_loop.Run();
    }
  }

  storage::SharedStorageManager* GetSharedStorageManager() {
    return static_cast<StoragePartitionImpl*>(
               browser_context()->GetDefaultStoragePartition())
        ->GetSharedStorageManager();
  }

  std::string GetExistingValue(const url::Origin& request_origin,
                               std::string key) {
    base::test::TestFuture<GetResult> future;
    auto* manager = GetSharedStorageManager();
    DCHECK(manager);
    std::u16string utf16_key;
    EXPECT_TRUE(base::UTF8ToUTF16(key.c_str(), key.size(), &utf16_key));
    manager->Get(request_origin, utf16_key, future.GetCallback());
    GetResult result = future.Take();
    EXPECT_EQ(result.result, OperationResult::kSuccess);
    std::string utf8_value;
    EXPECT_TRUE(base::UTF16ToUTF8(result.data.c_str(), result.data.size(),
                                  &utf8_value));
    return utf8_value;
  }

  bool ValueNotFound(const url::Origin& request_origin, std::string key) {
    base::test::TestFuture<GetResult> future;
    auto* manager = GetSharedStorageManager();
    DCHECK(manager);
    std::u16string utf16_key;
    EXPECT_TRUE(base::UTF8ToUTF16(key.c_str(), key.size(), &utf16_key));
    manager->Get(request_origin, utf16_key, future.GetCallback());
    GetResult result = future.Take();
    return result.result == OperationResult::kNotFound;
  }

  int Length(const url::Origin& request_origin) {
    base::test::TestFuture<int> future;
    auto* manager = GetSharedStorageManager();
    DCHECK(manager);
    manager->Length(request_origin, future.GetCallback());
    return future.Get();
  }

 protected:
  std::unique_ptr<TestSharedStorageHeaderObserver> observer_;
  std::unique_ptr<MockContentBrowserClient> mock_content_browser_client_;
  std::unique_ptr<NavigationSimulator> main_frame_navigation_simulator_;
  std::unique_ptr<NavigationSimulator> iframe_navigation_simulator_;
  scoped_refptr<NavigationOrDocumentHandle> handle_for_null_rfh_;

 private:
  base::test::ScopedFeatureList feature_list_;
  raw_ptr<content::ContentBrowserClient> old_content_browser_client_ = nullptr;
  std::vector<std::string> main_urls_;
  std::vector<std::string> child_urls_;
  size_t main_url_index_ = 0;
  size_t child_url_index_ = 0;
};

auto describe_test_case_type = [](const auto& info) {
  switch (info.param) {
    case TestCaseType::kNavigationRequestContextIframePermissionEnabled:
      return "IframeNavigationRequestPermissionEnabled";
    case TestCaseType::kNavigationRequestContextIframePermissionDisabled:
      return "IframeNavigationRequestPermissionDisabled";
    case TestCaseType::kNavigationRequestContextMainFrame:
      return "MainFrameNavigationRequest";
    case TestCaseType::kRenderFrameHostContextIframePermissionEnabled:
      return "IframeRFHPermissionEnabled";
    case TestCaseType::kRenderFrameHostContextIframePermissionDisabled:
      return "IframeRFHPermissionDisabled";
    case TestCaseType::kRenderFrameHostContextIframeArriveBeforeCommit:
      return "IframeRFHArriveBeforeCommit";
    case TestCaseType::kRenderFrameHostContextMainFramePermissionEnabled:
      return "MainFrameRFHPermissionEnabled";
    case TestCaseType::kRenderFrameHostContextMainFramePermissionDisabled:
      return "MainFrameRFHPermissionDisabled";
    case TestCaseType::kRenderFrameHostContextMainFrameArriveBeforeCommit:
      return "MainFrameRFHArriveBeforeCommit";
    case TestCaseType::kRenderFrameHostContextNoRenderFrameHost:
      return "NullRFH";
    default:
      NOTREACHED();
  }
};

}  // namespace

INSTANTIATE_TEST_SUITE_P(
    All,
    SharedStorageHeaderObserverTest,
    testing::Values(
        TestCaseType::kNavigationRequestContextIframePermissionEnabled,
        TestCaseType::kNavigationRequestContextIframePermissionDisabled,
        TestCaseType::kNavigationRequestContextMainFrame,
        TestCaseType::kRenderFrameHostContextIframePermissionEnabled,
        TestCaseType::kRenderFrameHostContextIframePermissionDisabled,
        TestCaseType::kRenderFrameHostContextIframeArriveBeforeCommit,
        TestCaseType::kRenderFrameHostContextMainFramePermissionEnabled,
        TestCaseType::kRenderFrameHostContextMainFramePermissionDisabled,
        TestCaseType::kRenderFrameHostContextMainFrameArriveBeforeCommit,
        TestCaseType::kRenderFrameHostContextNoRenderFrameHost),
    describe_test_case_type);

TEST_P(SharedStorageHeaderObserverTest, SharedStorageNotAllowed) {
  // Simulate disabling shared storage in user preferences.
  set_bypass_shared_storage_allowed_count(0);

  const url::Origin kOrigin1 = url::Origin::Create(GURL(kTestOrigin1));

  std::vector<MethodWithOptionsPtr> methods_with_options;
  methods_with_options.push_back(MojomClearMethod());
  methods_with_options.push_back(MojomSetMethod(/*key=*/u"key1",
                                                /*value=*/u"value1",
                                                /*ignore_if_present=*/false));
  methods_with_options.push_back(
      MojomAppendMethod(/*key=*/u"key1", /*value=*/u"value1"));
  methods_with_options.push_back(MojomSetMethod(/*key=*/u"key1",
                                                /*value=*/u"value2",
                                                /*ignore_if_present=*/true));
  methods_with_options.push_back(MojomSetMethod(/*key=*/u"key2",
                                                /*value=*/u"value2",
                                                /*ignore_if_present=*/false));
  methods_with_options.push_back(MojomDeleteMethod(/*key=*/u"key2"));

  // No operations are invoked because we've simulated shared storage being
  // disabled in user preferences.
  RunHeaderReceived(kOrigin1, CloneSharedStorageMethods(methods_with_options),
                    /*with_lock=*/std::nullopt);
  EXPECT_TRUE(observer_->header_results().empty());
  EXPECT_TRUE(observer_->operations().empty());
  EXPECT_EQ(Length(kOrigin1), 0);
}

TEST_P(SharedStorageHeaderObserverTest, Append_NoCapacity) {
  set_bypass_shared_storage_allowed_count(1);

  const url::Origin kOrigin1 = url::Origin::Create(GURL(kTestOrigin1));

  auto* manager = GetSharedStorageManager();
  DCHECK(manager);

  // Set the value at `key` to the maximum allowed length.
  base::test::TestFuture<OperationResult> future;
  std::u16string key(u"key1");
  std::u16string value(
      network::kMaxSharedStorageBytesPerOrigin / 2u - key.size(), u'a');
  manager->Set(kOrigin1, key, value, future.GetCallback(),
               storage::SharedStorageManager::SetBehavior::kDefault);
  OperationResult result = future.Take();

  ASSERT_EQ(result, OperationResult::kSet);

  std::vector<MethodWithOptionsPtr> methods_with_options;

  methods_with_options.push_back(MojomAppendMethod(key, /*value=*/u"a"));

  RunHeaderReceived(kOrigin1, CloneSharedStorageMethods(methods_with_options),
                    /*with_lock=*/std::nullopt);

  if (!ExpectSuccess()) {
    EXPECT_TRUE(observer_->header_results().empty());
    EXPECT_TRUE(observer_->operations().empty());
    return;
  }

  ASSERT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().back(), kOrigin1);

  observer_->WaitForOperations(1);

  // Expect that the database attempted the 'append' method but failed. This is
  // due to insufficient capacity for the origin.
  EXPECT_EQ(observer_->operations()[0],
            OperationAndResult(kOrigin1,
                               CloneSharedStorageMethods(methods_with_options),
                               /*with_lock=*/std::nullopt,
                               /*success=*/false));

  EXPECT_EQ(GetExistingValue(kOrigin1, base::UTF16ToUTF8(key)),
            base::UTF16ToUTF8(value));
}

TEST_P(SharedStorageHeaderObserverTest, Basic_SingleOrigin_AllSuccessful) {
  set_bypass_shared_storage_allowed_count(1);

  const url::Origin kOrigin1 = url::Origin::Create(GURL(kTestOrigin1));

  std::vector<MethodWithOptionsPtr> methods_with_options;
  methods_with_options.push_back(MojomClearMethod());
  methods_with_options.push_back(MojomSetMethod(/*key=*/u"key1",
                                                /*value=*/u"value1",
                                                /*ignore_if_present=*/false));
  methods_with_options.push_back(
      MojomAppendMethod(/*key=*/u"key1", /*value=*/u"value1"));
  methods_with_options.push_back(MojomSetMethod(/*key=*/u"key1",
                                                /*value=*/u"value2",
                                                /*ignore_if_present=*/true));
  methods_with_options.push_back(MojomSetMethod(/*key=*/u"key2",
                                                /*value=*/u"value2",
                                                /*ignore_if_present=*/false));
  methods_with_options.push_back(MojomDeleteMethod(/*key=*/u"key2"));

  RunHeaderReceived(kOrigin1, CloneSharedStorageMethods(methods_with_options),
                    /*with_lock=*/std::nullopt);

  if (!ExpectSuccess()) {
    EXPECT_TRUE(observer_->header_results().empty());
    EXPECT_TRUE(observer_->operations().empty());
    EXPECT_EQ(Length(kOrigin1), 0);
    return;
  }

  ASSERT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().back(), kOrigin1);

  observer_->WaitForOperations(1);
  EXPECT_EQ(observer_->operations()[0],
            OperationAndResult(kOrigin1,
                               CloneSharedStorageMethods(methods_with_options),
                               /*with_lock=*/std::nullopt,
                               /*success=*/true));

  EXPECT_EQ(GetExistingValue(kOrigin1, "key1"), "value1value1");
  EXPECT_TRUE(ValueNotFound(kOrigin1, "key2"));
  EXPECT_EQ(Length(kOrigin1), 1);
}

TEST_P(SharedStorageHeaderObserverTest, Basic_MultiOrigin_AllSuccessful) {
  set_bypass_shared_storage_allowed_count(3);

  const url::Origin kOrigin1 = url::Origin::Create(GURL(kTestOrigin1));
  const url::Origin kOrigin2 = url::Origin::Create(GURL(kTestOrigin2));
  const url::Origin kOrigin3 = url::Origin::Create(GURL(kTestOrigin3));

  std::vector<MethodWithOptionsPtr> methods_with_options1;
  methods_with_options1.push_back(MojomSetMethod(/*key=*/u"a", /*value=*/u"b",
                                                 /*ignore_if_present=*/false));

  std::vector<MethodWithOptionsPtr> methods_with_options2;
  methods_with_options2.push_back(
      MojomSetMethod(/*key=*/u"a", /*value=*/u"c", /*ignore_if_present=*/true));

  std::vector<MethodWithOptionsPtr> methods_with_options3;
  methods_with_options3.push_back(MojomDeleteMethod(/*key=*/u"a"));

  RunHeaderReceived(kOrigin1, CloneSharedStorageMethods(methods_with_options1),
                    /*with_lock=*/"lock1");

  if (!ExpectSuccess()) {
    RunHeaderReceived(kOrigin2,
                      CloneSharedStorageMethods(methods_with_options2),
                      /*with_lock=*/std::nullopt);
    RunHeaderReceived(kOrigin3,
                      CloneSharedStorageMethods(methods_with_options3),
                      /*with_lock=*/std::nullopt);
    EXPECT_TRUE(observer_->header_results().empty());
    EXPECT_TRUE(observer_->operations().empty());
    EXPECT_EQ(Length(kOrigin1), 0);
    EXPECT_EQ(Length(kOrigin2), 0);
    EXPECT_EQ(Length(kOrigin3), 0);
    return;
  }

  ASSERT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().back(), kOrigin1);

  RunHeaderReceived(kOrigin2, CloneSharedStorageMethods(methods_with_options2),
                    /*with_lock=*/std::nullopt);
  ASSERT_EQ(observer_->header_results().size(), 2u);
  EXPECT_EQ(observer_->header_results().back(), kOrigin2);

  RunHeaderReceived(kOrigin3, CloneSharedStorageMethods(methods_with_options3),
                    /*with_lock=*/std::nullopt);
  ASSERT_EQ(observer_->header_results().size(), 3u);
  EXPECT_EQ(observer_->header_results().back(), kOrigin3);

  observer_->WaitForOperations(3);

  // Operations on different origins don't affect each other.
  EXPECT_EQ(GetExistingValue(kOrigin1, "a"), "b");
  EXPECT_EQ(GetExistingValue(kOrigin2, "a"), "c");
  EXPECT_TRUE(ValueNotFound(kOrigin3, "a"));
  EXPECT_EQ(Length(kOrigin1), 1);
  EXPECT_EQ(Length(kOrigin2), 1);
  EXPECT_EQ(Length(kOrigin3), 0);
}

}  // namespace content
