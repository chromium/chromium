// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/shared_storage/shared_storage_header_observer.h"

#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/shared_storage_test_utils.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_shared_storage_header_observer.h"
#include "content/test/test_web_contents.h"
#include "services/network/public/mojom/optional_bool.mojom.h"
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
const char kChildOrigin[] = "https://child.test";
const char kTestOrigin1[] = "https://a.test";
const char kTestOrigin2[] = "https://b.test";
const char kTestOrigin3[] = "https://c.test";

using OperationPtr = network::mojom::SharedStorageOperationPtr;
using OperationType = network::mojom::SharedStorageOperationType;
using OperationResult = storage::SharedStorageManager::OperationResult;
using GetResult = storage::SharedStorageManager::GetResult;

enum class RenderFrameHostType {
  kMain = 0,
  kIframe = 1,
  kNull = 2,
};

[[nodiscard]] OperationPtr MakeSharedStorageOperationPtr(
    OperationType operation_type,
    absl::optional<std::string> key,
    absl::optional<std::string> value,
    absl::optional<bool> ignore_if_present) {
  return network::mojom::SharedStorageOperation::New(
      operation_type, std::move(key), std::move(value),
      AbslToMojomOptionalBool(ignore_if_present));
}

[[nodiscard]] std::vector<OperationPtr> MakeOperationVector(
    std::vector<std::tuple<OperationType,
                           absl::optional<std::string>,
                           absl::optional<std::string>,
                           absl::optional<bool>>> operation_tuples) {
  std::vector<OperationPtr> operations;
  for (const auto& operation_tuple : operation_tuples) {
    OperationPtr operation = MakeSharedStorageOperationPtr(
        std::get<0>(operation_tuple), std::get<1>(operation_tuple),
        std::get<2>(operation_tuple), std::get<3>(operation_tuple));
    operations.push_back(std::move(operation));
  }
  return operations;
}

[[nodiscard]] SharedStorageWriteOperationAndResult SetOperation(
    const url::Origin& request_origin,
    std::string key,
    std::string value,
    absl::optional<bool> ignore_if_present,
    OperationResult result) {
  return SharedStorageWriteOperationAndResult::SetOperation(
      request_origin, std::move(key), std::move(value), ignore_if_present,
      result);
}

[[nodiscard]] SharedStorageWriteOperationAndResult AppendOperation(
    const url::Origin& request_origin,
    std::string key,
    std::string value,
    OperationResult result) {
  return SharedStorageWriteOperationAndResult::AppendOperation(
      request_origin, std::move(key), std::move(value), result);
}

[[nodiscard]] SharedStorageWriteOperationAndResult DeleteOperation(
    const url::Origin& request_origin,
    std::string key,
    OperationResult result) {
  return SharedStorageWriteOperationAndResult::DeleteOperation(
      request_origin, std::move(key), result);
}

[[nodiscard]] SharedStorageWriteOperationAndResult ClearOperation(
    const url::Origin& request_origin,
    OperationResult result) {
  return SharedStorageWriteOperationAndResult::ClearOperation(request_origin,
                                                              result);
}

class SharedStorageHeaderObserverTest
    : public RenderViewHostTestHarness,
      public testing::WithParamInterface<RenderFrameHostType> {
 public:
  SharedStorageHeaderObserverTest() {
    feature_list_.InitAndEnableFeature(blink::features::kSharedStorageAPI);
  }

  ~SharedStorageHeaderObserverTest() override = default;

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    observer_ = std::make_unique<TestSharedStorageHeaderObserver>(
        browser_context()->GetDefaultStoragePartition());
    NavigateMainFrame(GURL(kMainOrigin));
  }

  void NavigateMainFrame(const GURL& url) {
    auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
        url, web_contents());
    simulator->Commit();
  }

  RenderFrameHost* CreateAndNavigateIFrame(RenderFrameHost* parent,
                                           const GURL& url,
                                           const std::string& name) {
    content::RenderFrameHost* rfh =
        content::RenderFrameHostTester::For(parent)->AppendChild(name);
    auto simulator =
        content::NavigationSimulator::CreateRendererInitiated(url, rfh);
    simulator->SetTransition(ui::PAGE_TRANSITION_MANUAL_SUBFRAME);
    simulator->Commit();
    return simulator->GetFinalRenderFrameHost();
  }

  RenderFrameHost* GetRequestFrame() {
    switch (GetParam()) {
      case RenderFrameHostType::kMain:
        return main_rfh();
      case RenderFrameHostType::kIframe:
        return CreateAndNavigateIFrame(main_rfh(), GURL(kChildOrigin),
                                       "myiframe");
      case RenderFrameHostType::kNull:
        return nullptr;
      default:
        NOTREACHED();
    }
    return nullptr;
  }

  void RunHeaderReceived(const url::Origin& request_origin,
                         std::vector<OperationPtr> operations) {
    base::RunLoop loop;
    observer_->HeaderReceived(request_origin, GetRequestFrame(),
                              std::move(operations), loop.QuitClosure());
    loop.Run();
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

 private:
  base::test::ScopedFeatureList feature_list_;
};

}  // namespace

INSTANTIATE_TEST_SUITE_P(All,
                         SharedStorageHeaderObserverTest,
                         testing::Values(RenderFrameHostType::kMain,
                                         RenderFrameHostType::kIframe,
                                         RenderFrameHostType::kNull),
                         [](const auto& info) {
                           switch (info.param) {
                             case RenderFrameHostType::kMain:
                               return "FromMainFrame";
                             case RenderFrameHostType::kIframe:
                               return "FromIFrame";
                             case RenderFrameHostType::kNull:
                               return "FromFrameNoLongerAlive";
                             default:
                               NOTREACHED();
                           }
                           return "NotReached";
                         });

TEST_P(SharedStorageHeaderObserverTest, SharedStorageNotAllowed) {
  // Simulate disabling shared storage in user preferences.
  SharedStorageHeaderObserver::GetBypassIsSharedStorageAllowedForTesting() =
      false;

  const url::Origin kOrigin1 = url::Origin::Create(GURL(kTestOrigin1));

  std::vector<OperationPtr> operations = MakeOperationVector(
      {std::make_tuple(OperationType::kClear, /*key*/ absl::nullopt,
                       /*value*/ absl::nullopt,
                       /*ignore_if_present*/ absl::nullopt),
       std::make_tuple(OperationType::kSet, "key1", "value1",
                       /*ignore_if_present*/ absl::nullopt),
       std::make_tuple(OperationType::kAppend, "key1", "value1",
                       /*ignore_if_present*/ absl::nullopt),
       std::make_tuple(OperationType::kSet, "key1", "value2",
                       /*ignore_if_present*/ true),
       std::make_tuple(OperationType::kSet, "key2", "value2",
                       /*ignore_if_present*/ false),
       std::make_tuple(OperationType::kDelete, "key2", /*value*/ absl::nullopt,
                       /*ignore_if_present*/ absl::nullopt)});

  // No operations are invoked because we've simulated shared storage being
  // disabled in user preferences.
  RunHeaderReceived(kOrigin1, std::move(operations));
  EXPECT_TRUE(observer_->header_results().empty());
  EXPECT_TRUE(observer_->operations().empty());
  EXPECT_EQ(Length(kOrigin1), 0);
}

TEST_P(SharedStorageHeaderObserverTest, Basic_SingleOrigin_AllSuccessful) {
  SharedStorageHeaderObserver::GetBypassIsSharedStorageAllowedForTesting() =
      true;

  const url::Origin kOrigin1 = url::Origin::Create(GURL(kTestOrigin1));

  std::vector<OperationPtr> operations = MakeOperationVector(
      {std::make_tuple(OperationType::kClear, /*key*/ absl::nullopt,
                       /*value*/ absl::nullopt,
                       /*ignore_if_present*/ absl::nullopt),
       std::make_tuple(OperationType::kSet, "key1", "value1",
                       /*ignore_if_present*/ absl::nullopt),
       std::make_tuple(OperationType::kAppend, "key1", "value1",
                       /*ignore_if_present*/ absl::nullopt),
       std::make_tuple(OperationType::kSet, "key1", "value2",
                       /*ignore_if_present*/ true),
       std::make_tuple(OperationType::kSet, "key2", "value2",
                       /*ignore_if_present*/ false),
       std::make_tuple(OperationType::kDelete, "key2", /*value*/ absl::nullopt,
                       /*ignore_if_present*/ absl::nullopt)});

  RunHeaderReceived(kOrigin1, std::move(operations));
  ASSERT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().back().first, kOrigin1);
  EXPECT_THAT(observer_->header_results().back().second,
              testing::ElementsAre(true, true, true, true, true, true));

  observer_->WaitForOperations(6);
  EXPECT_THAT(
      observer_->operations(),
      testing::ElementsAre(
          ClearOperation(kOrigin1, OperationResult::kSuccess),
          SetOperation(kOrigin1, "key1", "value1", absl::nullopt,
                       OperationResult::kSet),
          AppendOperation(kOrigin1, "key1", "value1", OperationResult::kSet),
          SetOperation(kOrigin1, "key1", "value2", true,
                       OperationResult::kIgnored),
          SetOperation(kOrigin1, "key2", "value2", false,
                       OperationResult::kSet),
          DeleteOperation(kOrigin1, "key2", OperationResult::kSuccess)));

  EXPECT_EQ(GetExistingValue(kOrigin1, "key1"), "value1value1");
  EXPECT_TRUE(ValueNotFound(kOrigin1, "key2"));
  EXPECT_EQ(Length(kOrigin1), 1);
}

TEST_P(SharedStorageHeaderObserverTest, Basic_MultiOrigin_AllSuccessful) {
  SharedStorageHeaderObserver::GetBypassIsSharedStorageAllowedForTesting() =
      true;

  const url::Origin kOrigin1 = url::Origin::Create(GURL(kTestOrigin1));
  const url::Origin kOrigin2 = url::Origin::Create(GURL(kTestOrigin2));
  const url::Origin kOrigin3 = url::Origin::Create(GURL(kTestOrigin3));

  std::vector<OperationPtr> operations1 = MakeOperationVector(
      {std::make_tuple(OperationType::kSet, "a", "b",
                       /*ignore_if_present*/ absl::nullopt)});
  std::vector<OperationPtr> operations2 =
      MakeOperationVector({std::make_tuple(OperationType::kSet, "a", "c",
                                           /*ignore_if_present*/ true)});
  std::vector<OperationPtr> operations3 = MakeOperationVector(
      {std::make_tuple(OperationType::kDelete, "a", /*value=*/absl::nullopt,
                       /*ignore_if_present*/ absl::nullopt)});

  RunHeaderReceived(kOrigin1, std::move(operations1));
  ASSERT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().back().first, kOrigin1);
  EXPECT_THAT(observer_->header_results().back().second,
              testing::ElementsAre(true));

  RunHeaderReceived(kOrigin2, std::move(operations2));
  ASSERT_EQ(observer_->header_results().size(), 2u);
  EXPECT_EQ(observer_->header_results().back().first, kOrigin2);
  EXPECT_THAT(observer_->header_results().back().second,
              testing::ElementsAre(true));

  RunHeaderReceived(kOrigin3, std::move(operations3));
  ASSERT_EQ(observer_->header_results().size(), 3u);
  EXPECT_EQ(observer_->header_results().back().first, kOrigin3);
  EXPECT_THAT(observer_->header_results().back().second,
              testing::ElementsAre(true));

  observer_->WaitForOperations(3);
  EXPECT_THAT(observer_->operations(),
              testing::ElementsAre(
                  SetOperation(kOrigin1, "a", "b", absl::nullopt,
                               OperationResult::kSet),
                  SetOperation(kOrigin2, "a", "c", true, OperationResult::kSet),
                  DeleteOperation(kOrigin3, "a", OperationResult::kSuccess)));

  // Operations on different origins don't affect each other.
  EXPECT_EQ(GetExistingValue(kOrigin1, "a"), "b");
  EXPECT_EQ(GetExistingValue(kOrigin2, "a"), "c");
  EXPECT_TRUE(ValueNotFound(kOrigin3, "a"));
  EXPECT_EQ(Length(kOrigin1), 1);
  EXPECT_EQ(Length(kOrigin2), 1);
  EXPECT_EQ(Length(kOrigin3), 0);
}

TEST_P(SharedStorageHeaderObserverTest, SkipMissingParams) {
  SharedStorageHeaderObserver::GetBypassIsSharedStorageAllowedForTesting() =
      true;

  const url::Origin kOrigin1 = url::Origin::Create(GURL(kTestOrigin1));

  std::vector<OperationPtr> operations = MakeOperationVector({
      std::make_tuple(OperationType::kClear, /*key*/ absl::nullopt,
                      /*value*/ absl::nullopt,
                      /*ignore_if_present*/ absl::nullopt),  // success
      std::make_tuple(OperationType::kSet, "a", /*value*/ absl::nullopt,
                      /*ignore_if_present*/ false),  // fail
      std::make_tuple(OperationType::kSet, /*key*/ absl::nullopt, "b",
                      /*ignore_if_present*/ true),  // fail
      std::make_tuple(OperationType::kSet, /*key*/ absl::nullopt,
                      /*value*/ absl::nullopt,
                      /*ignore_if_present*/ true),  // fail
      std::make_tuple(OperationType::kAppend, "a", /*value*/ absl::nullopt,
                      /*ignore_if_present*/ absl::nullopt),  // fail
      std::make_tuple(OperationType::kAppend, "a", "b",
                      /*ignore_if_present*/ absl::nullopt),  // success
      std::make_tuple(OperationType::kAppend, /*key*/ absl::nullopt, "b",
                      /*ignore_if_present*/ absl::nullopt),  // fail
      std::make_tuple(OperationType::kAppend, /*key*/ absl::nullopt,
                      /*value*/ absl::nullopt,
                      /*ignore_if_present*/ absl::nullopt),  // fail
      std::make_tuple(OperationType::kSet, "a", "c",
                      /*ignore_if_present*/ true),  // success
      std::make_tuple(OperationType::kSet, "d", "c",
                      /*ignore_if_present*/ absl::nullopt),  // success
      std::make_tuple(OperationType::kDelete, /*key*/ absl::nullopt,
                      /*value*/ absl::nullopt,
                      /*ignore_if_present*/ absl::nullopt),  // fail
      std::make_tuple(OperationType::kDelete, "anything",
                      /*value*/ absl::nullopt,
                      /*ignore_if_present*/ absl::nullopt),  // success
  });

  RunHeaderReceived(kOrigin1, std::move(operations));
  ASSERT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().back().first, kOrigin1);
  EXPECT_THAT(observer_->header_results().back().second,
              testing::ElementsAre(true, false, false, false, false, true,
                                   false, false, true, true, false, true));

  observer_->WaitForOperations(5);
  EXPECT_THAT(
      observer_->operations(),
      testing::ElementsAre(
          ClearOperation(kOrigin1, OperationResult::kSuccess),
          AppendOperation(kOrigin1, "a", "b", OperationResult::kSet),
          SetOperation(kOrigin1, "a", "c", true, OperationResult::kIgnored),
          SetOperation(kOrigin1, "d", "c", absl::nullopt,
                       OperationResult::kSet),
          DeleteOperation(kOrigin1, "anything", OperationResult::kSuccess)));

  EXPECT_EQ(GetExistingValue(kOrigin1, "a"), "b");
  EXPECT_EQ(GetExistingValue(kOrigin1, "d"), "c");
  EXPECT_TRUE(ValueNotFound(kOrigin1, "anything"));
  EXPECT_EQ(Length(kOrigin1), 2);
}

TEST_P(SharedStorageHeaderObserverTest, SkipInvalidParams) {
  SharedStorageHeaderObserver::GetBypassIsSharedStorageAllowedForTesting() =
      true;

  const url::Origin kOrigin1 = url::Origin::Create(GURL(kTestOrigin1));
  const std::string kLong(1025, 'x');

  std::vector<OperationPtr> operations = MakeOperationVector({
      std::make_tuple(OperationType::kClear, /*key*/ absl::nullopt,
                      /*value*/ absl::nullopt,
                      /*ignore_if_present*/ absl::nullopt),  // success
      std::make_tuple(OperationType::kSet, "a", kLong,
                      /*ignore_if_present*/ false),  // fail
      std::make_tuple(OperationType::kSet, kLong, "b",
                      /*ignore_if_present*/ true),  // fail
      std::make_tuple(OperationType::kSet, "", "b",
                      /*ignore_if_present*/ true),  // fail
      std::make_tuple(OperationType::kSet, kLong, kLong,
                      /*ignore_if_present*/ true),  // fail
      std::make_tuple(OperationType::kSet, "", kLong,
                      /*ignore_if_present*/ true),  // fail
      std::make_tuple(OperationType::kAppend, "a", kLong,
                      /*ignore_if_present*/ absl::nullopt),  // fail
      std::make_tuple(OperationType::kAppend, "a", "b",
                      /*ignore_if_present*/ absl::nullopt),  // success
      std::make_tuple(OperationType::kAppend, kLong, "b",
                      /*ignore_if_present*/ absl::nullopt),  // fail
      std::make_tuple(OperationType::kAppend, "", "b",
                      /*ignore_if_present*/ absl::nullopt),  // fail
      std::make_tuple(OperationType::kAppend, kLong, kLong,
                      /*ignore_if_present*/ absl::nullopt),  // fail
      std::make_tuple(OperationType::kAppend, "", kLong,
                      /*ignore_if_present*/ absl::nullopt),  // fail
      std::make_tuple(OperationType::kSet, "a", "c",
                      /*ignore_if_present*/ true),  // success
      std::make_tuple(OperationType::kSet, "d", "c",
                      /*ignore_if_present*/ absl::nullopt),  // success
      std::make_tuple(OperationType::kDelete, kLong,
                      /*value*/ absl::nullopt,
                      /*ignore_if_present*/ absl::nullopt),  // fail
      std::make_tuple(OperationType::kDelete, "",
                      /*value*/ absl::nullopt,
                      /*ignore_if_present*/ absl::nullopt),  // fail
      std::make_tuple(OperationType::kDelete, "anything",
                      /*value*/ absl::nullopt,
                      /*ignore_if_present*/ absl::nullopt),  // success
  });

  RunHeaderReceived(kOrigin1, std::move(operations));
  ASSERT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().back().first, kOrigin1);
  EXPECT_THAT(observer_->header_results().back().second,
              testing::ElementsAre(true, false, false, false, false, false,
                                   false, true, false, false, false, false,
                                   true, true, false, false, true));

  observer_->WaitForOperations(5);
  EXPECT_THAT(
      observer_->operations(),
      testing::ElementsAre(
          ClearOperation(kOrigin1, OperationResult::kSuccess),
          AppendOperation(kOrigin1, "a", "b", OperationResult::kSet),
          SetOperation(kOrigin1, "a", "c", true, OperationResult::kIgnored),
          SetOperation(kOrigin1, "d", "c", absl::nullopt,
                       OperationResult::kSet),
          DeleteOperation(kOrigin1, "anything", OperationResult::kSuccess)));

  EXPECT_EQ(GetExistingValue(kOrigin1, "a"), "b");
  EXPECT_EQ(GetExistingValue(kOrigin1, "d"), "c");
  EXPECT_TRUE(ValueNotFound(kOrigin1, "anything"));
  EXPECT_EQ(Length(kOrigin1), 2);
}

TEST_P(SharedStorageHeaderObserverTest, IgnoreExtraParams) {
  SharedStorageHeaderObserver::GetBypassIsSharedStorageAllowedForTesting() =
      true;

  const url::Origin kOrigin1 = url::Origin::Create(GURL(kTestOrigin1));

  std::vector<OperationPtr> operations = MakeOperationVector(
      {std::make_tuple(OperationType::kClear, "key1", "value2",
                       /*ignore_if_present*/ true),  // extra params
       std::make_tuple(OperationType::kSet, "key1", "value1",
                       /*ignore_if_present*/ absl::nullopt),
       std::make_tuple(OperationType::kAppend, "key1", "value1",
                       /*ignore_if_present*/ false),  // extra param
       std::make_tuple(OperationType::kSet, "key1", "value2",
                       /*ignore_if_present*/ true),
       std::make_tuple(OperationType::kSet, "key2", "value2",
                       /*ignore_if_present*/ false),
       std::make_tuple(OperationType::kDelete, "key2", "value2",
                       /*ignore_if_present*/ false)});  // extra params

  RunHeaderReceived(kOrigin1, std::move(operations));
  ASSERT_EQ(observer_->header_results().size(), 1u);
  EXPECT_EQ(observer_->header_results().back().first, kOrigin1);
  EXPECT_THAT(observer_->header_results().back().second,
              testing::ElementsAre(true, true, true, true, true, true));

  // Superfluous parameters are omitted.
  observer_->WaitForOperations(6);
  EXPECT_THAT(
      observer_->operations(),
      testing::ElementsAre(
          ClearOperation(kOrigin1, OperationResult::kSuccess),
          SetOperation(kOrigin1, "key1", "value1", absl::nullopt,
                       OperationResult::kSet),
          AppendOperation(kOrigin1, "key1", "value1", OperationResult::kSet),
          SetOperation(kOrigin1, "key1", "value2", true,
                       OperationResult::kIgnored),
          SetOperation(kOrigin1, "key2", "value2", false,
                       OperationResult::kSet),
          DeleteOperation(kOrigin1, "key2", OperationResult::kSuccess)));

  EXPECT_EQ(GetExistingValue(kOrigin1, "key1"), "value1value1");
  EXPECT_TRUE(ValueNotFound(kOrigin1, "key2"));
  EXPECT_EQ(Length(kOrigin1), 1);
}

}  // namespace content
