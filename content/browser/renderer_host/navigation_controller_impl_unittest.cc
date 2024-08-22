// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/navigation_controller_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>
#include <tuple>
#include <utility>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/browser_url_handler_impl.h"
#include "content/browser/renderer_host/frame_navigation_entry.h"
#include "content/browser/renderer_host/navigation_entry_impl.h"
#include "content/browser/renderer_host/navigation_entry_restore_context_impl.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/navigator.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/content_navigation_policy.h"
#include "content/common/frame.mojom.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/page_type.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/fake_local_frame.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/scoped_web_ui_controller_factory_registration.h"
#include "content/public/test/test_navigation_ui_data.h"
#include "content/public/test/test_utils.h"
#include "content/test/navigation_simulator_impl.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "skia/ext/platform_canvas.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/frame/frame_policy.h"
#include "third_party/blink/public/common/page/browsing_context_group_info.h"
#include "third_party/blink/public/common/page_state/page_state.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/frame/frame_owner_properties.mojom.h"
#include "third_party/blink/public/mojom/frame/remote_frame.mojom.h"
#include "third_party/blink/public/mojom/frame/user_activation_update_types.mojom.h"

using base::Time;

namespace {

base::Time InMicrosecondsSinceEpoch(int64_t us) {
  return base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(us));
}

// Creates an image with a 1x1 SkBitmap of the specified |color|.
gfx::Image CreateImage(SkColor color) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(1, 1);
  bitmap.eraseColor(color);
  return gfx::Image::CreateFrom1xBitmap(bitmap);
}

// Returns true if images |a| and |b| have the same pixel data.
bool DoImagesMatch(const gfx::Image& a, const gfx::Image& b) {
  // Assume that if the 1x bitmaps match, the images match.
  SkBitmap a_bitmap = a.AsBitmap();
  SkBitmap b_bitmap = b.AsBitmap();

  if (a_bitmap.width() != b_bitmap.width() ||
      a_bitmap.height() != b_bitmap.height()) {
    return false;
  }

  // memcmp(nullptr, nullptr, 0) is undefined, so empty bitmaps must be
  // special-cased.
  return a_bitmap.computeByteSize() == 0 ||
         memcmp(a_bitmap.getPixels(), b_bitmap.getPixels(),
                a_bitmap.computeByteSize()) == 0;
}

class MockPageBroadcast : public blink::mojom::PageBroadcast {
 public:
  explicit MockPageBroadcast() : receiver_(this) {}

  ~MockPageBroadcast() override { receiver_.FlushForTesting(); }

  MOCK_METHOD(void,
              SetPageLifecycleState,
              (blink::mojom::PageLifecycleStatePtr state,
               blink::mojom::PageRestoreParamsPtr page_restore_params,
               SetPageLifecycleStateCallback callback),
              (override));
  MOCK_METHOD(void, AudioStateChanged, (bool is_audio_playing), (override));
  MOCK_METHOD(void,
              ActivatePrerenderedPage,
              (blink::mojom::PrerenderPageActivationParamsPtr
                   prerender_page_activation_params,
               ActivatePrerenderedPageCallback callback),
              (override));
  MOCK_METHOD(void,
              UpdateWebPreferences,
              (const ::blink::web_pref::WebPreferences& preferences),
              (override));
  MOCK_METHOD(void,
              UpdateRendererPreferences,
              (const ::blink::RendererPreferences& preferences),
              (override));
  MOCK_METHOD(void,
              SetHistoryOffsetAndLength,
              (int32_t offset, int32_t length),
              (override));
  MOCK_METHOD(void,
              SetPageBaseBackgroundColor,
              (std::optional<SkColor> color),
              (override));
  MOCK_METHOD(void,
              UpdateColorProviders,
              (const ::blink::ColorProviderColorMaps& color_provider_colors),
              (override));
  MOCK_METHOD(
      void,
      CreateRemoteMainFrame,
      (const blink::RemoteFrameToken& token,
       const std::optional<blink::FrameToken>& opener_frame_token,
       blink::mojom::FrameReplicationStatePtr replication_state,
       bool is_loading,
       const base::UnguessableToken& devtools_frame_token,
       blink::mojom::RemoteFrameInterfacesFromBrowserPtr
           remote_frame_interfaces,
       blink::mojom::RemoteMainFrameInterfacesPtr remote_main_frame_interfaces),
      (override));

  MOCK_METHOD(
      void,
      UpdatePageBrowsingContextGroup,
      (const blink::BrowsingContextGroupInfo& browsing_context_group_info),
      (override));

  MOCK_METHOD(void,
              SetPageAttributionSupport,
              (network::mojom::AttributionSupport support),
              (override));

  mojo::PendingAssociatedRemote<blink::mojom::PageBroadcast> GetRemote() {
    return receiver_.BindNewEndpointAndPassDedicatedRemote();
  }

 private:
  mojo::AssociatedReceiver<blink::mojom::PageBroadcast> receiver_;
};

}  // namespace

namespace content {

// TimeSmoother tests ----------------------------------------------------------

// With no duplicates, GetSmoothedTime should be the identity
// function.
TEST(TimeSmoother, Basic) {
  NavigationControllerImpl::TimeSmoother smoother;
  for (int64_t i = 1; i < 1000; ++i) {
    base::Time t = InMicrosecondsSinceEpoch(i);
    EXPECT_EQ(t, smoother.GetSmoothedTime(t));
  }
}

// With a single duplicate and timestamps thereafter increasing by one
// microsecond, the smoothed time should always be one behind.
TEST(TimeSmoother, SingleDuplicate) {
  NavigationControllerImpl::TimeSmoother smoother;
  base::Time t = InMicrosecondsSinceEpoch(1);
  EXPECT_EQ(t, smoother.GetSmoothedTime(t));
  for (int64_t i = 1; i < 1000; ++i) {
    base::Time expected_t = InMicrosecondsSinceEpoch(i + 1);
    t = InMicrosecondsSinceEpoch(i);
    EXPECT_EQ(expected_t, smoother.GetSmoothedTime(t));
  }
}

// With k duplicates and timestamps thereafter increasing by one
// microsecond, the smoothed time should always be k behind.
TEST(TimeSmoother, ManyDuplicates) {
  const int64_t kNumDuplicates = 100;
  NavigationControllerImpl::TimeSmoother smoother;
  base::Time t = InMicrosecondsSinceEpoch(1);
  for (int64_t i = 0; i < kNumDuplicates; ++i) {
    base::Time expected_t = InMicrosecondsSinceEpoch(i + 1);
    EXPECT_EQ(expected_t, smoother.GetSmoothedTime(t));
  }
  for (int64_t i = 1; i < 1000; ++i) {
    base::Time expected_t = InMicrosecondsSinceEpoch(i + kNumDuplicates);
    t = InMicrosecondsSinceEpoch(i);
    EXPECT_EQ(expected_t, smoother.GetSmoothedTime(t));
  }
}

// If the clock jumps far back enough after a run of duplicates, it
// should immediately jump to that value.
TEST(TimeSmoother, ClockBackwardsJump) {
  const int64_t kNumDuplicates = 100;
  NavigationControllerImpl::TimeSmoother smoother;
  base::Time t = InMicrosecondsSinceEpoch(1000);
  for (int64_t i = 0; i < kNumDuplicates; ++i) {
    base::Time expected_t = InMicrosecondsSinceEpoch(i + 1000);
    EXPECT_EQ(expected_t, smoother.GetSmoothedTime(t));
  }
  t = InMicrosecondsSinceEpoch(500);
  EXPECT_EQ(t, smoother.GetSmoothedTime(t));
}

// NavigationControllerTest ----------------------------------------------------

class NavigationControllerTest : public RenderViewHostImplTestHarness,
                                 public WebContentsObserver {
 public:
  NavigationControllerTest() {
    // Disable BackForward cache size overwritten by
    // `kBackForwardCacheSize` so that it won't break some tests assumption.
    scoped_feature_list_.InitWithFeaturesAndParameters({},
                                                       {kBackForwardCacheSize});
  }

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    WebContents* web_contents = RenderViewHostImplTestHarness::web_contents();
    ASSERT_TRUE(web_contents);  // The WebContents should be created by now.
    WebContentsObserver::Observe(web_contents);
  }

  // WebContentsObserver:
  void DidStartNavigation(NavigationHandle* navigation) override {
    navigated_url_ = navigation->GetURL();
    last_reload_type_ = navigation->GetReloadType();
  }

  void NavigationEntryCommitted(
      const LoadCommittedDetails& load_details) override {
    navigation_entry_committed_counter_++;
  }

  void NavigationEntryChanged(
      const EntryChangedDetails& load_details) override {
    navigation_entry_changed_counter_++;
  }

  void NavigationListPruned(const PrunedDetails& details) override {
    navigation_list_pruned_counter_++;
    last_navigation_entry_pruned_details_ = details;
  }

  void NavigationEntriesDeleted() override {
    navigation_entries_deleted_counter_++;
  }

  const GURL& navigated_url() const { return navigated_url_; }

  NavigationControllerImpl& controller_impl() {
    return static_cast<NavigationControllerImpl&>(controller());
  }

  bool HasNavigationRequest() {
    return contents()->GetPrimaryFrameTree().root()->navigation_request() !=
           nullptr;
  }

  const GURL GetLastNavigationURL() {
    NavigationRequest* navigation_request =
        contents()->GetPrimaryFrameTree().root()->navigation_request();
    CHECK(navigation_request);
    return navigation_request->common_params().url;
  }

  TestRenderFrameHost* GetNavigatingRenderFrameHost() {
    return AreAllSitesIsolatedForTesting()
               ? contents()->GetSpeculativePrimaryMainFrame()
               : contents()->GetPrimaryMainFrame();
  }

  FrameTreeNode* root_ftn() { return contents()->GetPrimaryFrameTree().root(); }

  void BeforeFormRepostWarningShow() override { form_repost_counter_++; }

 protected:
  GURL navigated_url_;
  size_t navigation_entry_committed_counter_ = 0;
  size_t navigation_entry_changed_counter_ = 0;
  size_t navigation_list_pruned_counter_ = 0;
  size_t navigation_entries_deleted_counter_ = 0;
  size_t form_repost_counter_ = 0;
  PrunedDetails last_navigation_entry_pruned_details_;
  ReloadType last_reload_type_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class TestWebContentsDelegate : public WebContentsDelegate {
 public:
  TestWebContentsDelegate() = default;

  int navigation_state_change_count() { return navigation_state_change_count_; }

  // Keep track of whether the tab has notified us of a navigation state change.
  void NavigationStateChanged(WebContents* source,
                              InvalidateTypes changed_flags) override {
    navigation_state_change_count_++;
  }

 private:
  // The number of times NavigationStateChanged has been called.
  int navigation_state_change_count_ = 0;
};

// Observer that records the LoadCommittedDetails from the most recent commit.
class LoadCommittedDetailsObserver : public WebContentsObserver {
 public:
  // Observes navigation for the specified |web_contents|.
  explicit LoadCommittedDetailsObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  NavigationType navigation_type() { return navigation_type_; }
  const GURL& previous_primary_main_frame_url() {
    return previous_primary_main_frame_url_;
  }
  ReloadType reload_type() { return reload_type_; }
  bool is_same_document() { return is_same_document_; }
  bool is_main_frame() { return is_main_frame_; }
  bool did_replace_entry() { return did_replace_entry_; }
  bool has_navigation_ui_data() { return has_navigation_ui_data_; }

 private:
  void DidFinishNavigation(NavigationHandle* navigation_handle) override {
    if (!navigation_handle->HasCommitted())
      return;

    navigation_type_ =
        NavigationRequest::From(navigation_handle)->navigation_type();
    previous_primary_main_frame_url_ =
        navigation_handle->GetPreviousPrimaryMainFrameURL();
    reload_type_ = navigation_handle->GetReloadType();
    is_same_document_ = navigation_handle->IsSameDocument();
    is_main_frame_ = navigation_handle->IsInMainFrame();
    did_replace_entry_ = navigation_handle->DidReplaceEntry();
    has_navigation_ui_data_ = navigation_handle->GetNavigationUIData();
  }

  NavigationType navigation_type_ = NAVIGATION_TYPE_UNKNOWN;
  GURL previous_primary_main_frame_url_;
  ReloadType reload_type_ = ReloadType::NONE;
  bool is_same_document_ = false;
  bool is_main_frame_ = false;
  bool did_replace_entry_ = false;
  bool has_navigation_ui_data_ = false;
};

// "Legacy" class that was used to run NavigationControllerTest with the now
// defunct --enable-browser-side-navigation flag.
// TODO(clamy): Make those regular NavigationControllerTests.
class NavigationControllerTestWithBrowserSideNavigation
    : public NavigationControllerTest {
 public:
  void SetUp() override { NavigationControllerTest::SetUp(); }
};

// -----------------------------------------------------------------------------

TEST_F(NavigationControllerTest, Defaults) {
  NavigationControllerImpl& controller = controller_impl();

  EXPECT_FALSE(controller.GetPendingEntry());
  EXPECT_TRUE(!controller.GetVisibleEntry() ||
              controller.GetVisibleEntry()->IsInitialEntry());
  EXPECT_TRUE(!controller.GetLastCommittedEntry() ||
              controller.GetLastCommittedEntry()->IsInitialEntry());
  EXPECT_EQ(controller.GetPendingEntryIndex(), -1);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), 0);
  EXPECT_EQ(controller.GetEntryCount(), 1);
  EXPECT_FALSE(controller.CanGoBack());
  EXPECT_FALSE(controller.CanGoForward());
}

TEST_F(NavigationControllerTest, GoToOffset) {
  NavigationControllerImpl& controller = controller_impl();

  const int kNumUrls = 5;
  std::vector<GURL> urls(kNumUrls);
  for (int i = 0; i < kNumUrls; ++i) {
    urls[i] = GURL(base::StringPrintf("http://www.a.com/%d", i));
  }

  NavigationSimulator::NavigateAndCommitFromDocument(urls[0], main_test_rfh());
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;
  EXPECT_EQ(urls[0], controller.GetVisibleEntry()->GetVirtualURL());
  EXPECT_FALSE(controller.CanGoBack());
  EXPECT_FALSE(controller.CanGoForward());
  EXPECT_FALSE(controller.CanGoToOffset(1));

  for (int i = 1; i <= 4; ++i) {
    NavigationSimulator::NavigateAndCommitFromDocument(urls[i],
                                                       main_test_rfh());
    EXPECT_EQ(1U, navigation_entry_committed_counter_);
    navigation_entry_committed_counter_ = 0;
    EXPECT_EQ(urls[i], controller.GetVisibleEntry()->GetVirtualURL());
    EXPECT_TRUE(controller.CanGoToOffset(-i));
    EXPECT_FALSE(controller.CanGoToOffset(-(i + 1)));
    EXPECT_FALSE(controller.CanGoToOffset(1));
  }

  // We have loaded 5 pages, and are currently at the last-loaded page.
  int url_index = 4;

  enum Tests {
    GO_TO_MIDDLE_PAGE = -2,
    GO_FORWARDS = 1,
    GO_BACKWARDS = -1,
    GO_TO_BEGINNING = -2,
    GO_TO_END = 4,
    NUM_TESTS = 5,
  };

  const std::array<int, NUM_TESTS> test_offsets = {
      GO_TO_MIDDLE_PAGE, GO_FORWARDS, GO_BACKWARDS, GO_TO_BEGINNING, GO_TO_END};

  if (IsBackForwardCacheEnabled()) {
    // The `navigation_entry_committed_counter_` checks below currently fail on
    // the linux-bfcache-rel bot with bfcache enabled, so return early.
    // TODO(crbug.com/40780539): re-enable this test.
    return;
  }

  for (int test = 0; test < NUM_TESTS; ++test) {
    int offset = test_offsets[test];
    auto navigation = NavigationSimulator::CreateHistoryNavigation(
        offset, contents(), false /* is_renderer_initiated */);
    navigation->Start();
    url_index += offset;
    // Check that the GoToOffset will land on the expected page.
    EXPECT_EQ(urls[url_index], controller.GetPendingEntry()->GetVirtualURL());
    navigation->Commit();
    EXPECT_EQ(1U, navigation_entry_committed_counter_);
    navigation_entry_committed_counter_ = 0;
    // Check that we can go to any valid offset into the history.
    for (size_t j = 0; j < urls.size(); ++j)
      EXPECT_TRUE(controller.CanGoToOffset(j - url_index));
    // Check that we can't go beyond the beginning or end of the history.
    EXPECT_FALSE(controller.CanGoToOffset(-(url_index + 1)));
    EXPECT_FALSE(controller.CanGoToOffset(urls.size() - url_index));
  }
}

TEST_F(NavigationControllerTest, LoadURL) {
  NavigationControllerImpl& controller = controller_impl();

  const GURL url1("http://foo1");
  const GURL url2("http://foo2");

  controller.LoadURL(url1, Referrer(), ui::PAGE_TRANSITION_TYPED,
                     std::string());
  // Creating a pending notification should not have issued any of the
  // notifications we're listening for.
  EXPECT_EQ(0U, navigation_entry_changed_counter_);
  EXPECT_EQ(0U, navigation_list_pruned_counter_);

  // The load should now be pending.
  EXPECT_EQ(controller.GetEntryCount(), 1);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), 0);
  EXPECT_EQ(controller.GetPendingEntryIndex(), -1);
  EXPECT_TRUE(controller.GetLastCommittedEntry());

  ASSERT_TRUE(controller.GetPendingEntry());
  EXPECT_EQ(controller.GetPendingEntry(), controller.GetVisibleEntry());
  EXPECT_FALSE(controller.CanGoBack());
  EXPECT_FALSE(controller.CanGoForward());

  // Neither the timestamp nor the status code should have been set yet.
  EXPECT_TRUE(controller.GetPendingEntry()->GetTimestamp().is_null());
  EXPECT_EQ(0, controller.GetPendingEntry()->GetHttpStatusCode());

  // We should have gotten no notifications from the preceeding checks.
  EXPECT_EQ(0U, navigation_entry_changed_counter_);
  EXPECT_EQ(0U, navigation_list_pruned_counter_);

  auto navigation1 =
      NavigationSimulator::CreateFromPending(contents()->GetController());
  navigation1->Commit();
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;

  // The load should now be committed.
  EXPECT_EQ(controller.GetEntryCount(), 1);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), 0);
  EXPECT_EQ(controller.GetPendingEntryIndex(), -1);
  EXPECT_FALSE(controller.GetLastCommittedEntry()->IsInitialEntry());
  EXPECT_FALSE(controller.GetPendingEntry());
  ASSERT_FALSE(controller.GetVisibleEntry()->IsInitialEntry());
  EXPECT_FALSE(controller.CanGoBack());
  EXPECT_FALSE(controller.CanGoForward());
  EXPECT_TRUE(controller.GetLastCommittedEntry()
                  ->GetFrameEntry(root_ftn())
                  ->bindings()
                  ->empty());

  // The timestamp should have been set.
  EXPECT_FALSE(controller.GetVisibleEntry()->GetTimestamp().is_null());

  // Simulate a user gesture so that the above entry is not marked to be skipped
  // on back.
  main_test_rfh()->frame_tree_node()->UpdateUserActivationState(
      blink::mojom::UserActivationUpdateType::kNotifyActivation,
      blink::mojom::UserActivationNotificationType::kTest);

  // Load another...
  controller.LoadURL(url2, Referrer(), ui::PAGE_TRANSITION_TYPED,
                     std::string());

  // The load should now be pending.
  EXPECT_EQ(controller.GetEntryCount(), 1);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), 0);
  EXPECT_EQ(controller.GetPendingEntryIndex(), -1);
  EXPECT_FALSE(controller.GetLastCommittedEntry()->IsInitialEntry());
  ASSERT_TRUE(controller.GetPendingEntry());
  EXPECT_EQ(controller.GetPendingEntry(), controller.GetVisibleEntry());
  // TODO(darin): maybe this should really be true?
  EXPECT_FALSE(controller.CanGoBack());
  EXPECT_FALSE(controller.CanGoForward());

  EXPECT_TRUE(controller.GetPendingEntry()->GetTimestamp().is_null());

  auto navigation2 =
      NavigationSimulator::CreateFromPending(contents()->GetController());
  navigation2->Commit();
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;

  // The load should now be committed.
  EXPECT_EQ(controller.GetEntryCount(), 2);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), 1);
  EXPECT_EQ(controller.GetPendingEntryIndex(), -1);
  EXPECT_FALSE(controller.GetLastCommittedEntry()->IsInitialEntry());
  EXPECT_FALSE(controller.GetPendingEntry());
  ASSERT_FALSE(controller.GetVisibleEntry()->IsInitialEntry());
  EXPECT_TRUE(controller.CanGoBack());
  EXPECT_FALSE(controller.CanGoForward());

  EXPECT_FALSE(controller.GetVisibleEntry()->GetTimestamp().is_null());
}

namespace {

base::Time GetFixedTime(base::Time time) {
  return time;
}

}  // namespace

TEST_F(NavigationControllerTest, LoadURLSameTime) {
  NavigationControllerImpl& controller = controller_impl();

  // Set the clock to always return a timestamp of 1.
  controller.SetGetTimestampCallbackForTest(
      base::BindRepeating(&GetFixedTime, InMicrosecondsSinceEpoch(1)));

  const GURL url1("http://foo1");
  const GURL url2("http://foo2");

  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), url1);
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;

  // Load another...
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), url2);
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;

  // The two loads should now be committed.
  ASSERT_EQ(controller.GetEntryCount(), 2);

  // Timestamps should be distinct despite the clock returning the
  // same value.
  base::Time t;
  t = controller.GetEntryAtIndex(0)->GetTimestamp();
  EXPECT_EQ(1u, t.ToDeltaSinceWindowsEpoch().InMicroseconds());
  t = controller.GetEntryAtIndex(1)->GetTimestamp();
  EXPECT_EQ(2u, t.ToDeltaSinceWindowsEpoch().InMicroseconds());
}

void CheckNavigationEntryMatchLoadParams(
    const NavigationController::LoadURLParams& load_params,
    NavigationEntryImpl* entry) {
  EXPECT_EQ(load_params.url, entry->GetURL());
  EXPECT_EQ(load_params.referrer.url, entry->GetReferrer().url);
  EXPECT_EQ(load_params.referrer.policy, entry->GetReferrer().policy);
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      entry->GetTransitionType(), load_params.transition_type));
  std::string extra_headers_crlf;
  base::ReplaceChars(load_params.extra_headers, "\n", "\r\n",
                     &extra_headers_crlf);
  EXPECT_EQ(extra_headers_crlf, entry->extra_headers());

  EXPECT_EQ(load_params.is_renderer_initiated, entry->is_renderer_initiated());
  EXPECT_EQ(load_params.base_url_for_data_url, entry->GetBaseURLForDataURL());
  if (!load_params.virtual_url_for_special_cases.is_empty()) {
    EXPECT_EQ(load_params.virtual_url_for_special_cases,
              entry->GetVirtualURL());
  }
#if BUILDFLAG(IS_ANDROID)
  EXPECT_EQ(load_params.data_url_as_string, entry->GetDataURLAsString());
#endif
  if (NavigationController::UA_OVERRIDE_INHERIT !=
      load_params.override_user_agent) {
    bool should_override = (NavigationController::UA_OVERRIDE_TRUE ==
                            load_params.override_user_agent);
    EXPECT_EQ(should_override, entry->GetIsOverridingUserAgent());
  }
  EXPECT_EQ(load_params.post_data, entry->GetPostData());
  EXPECT_EQ(load_params.reload_type, entry->reload_type());
}

TEST_F(NavigationControllerTest, LoadURLWithParams) {
  // Start a navigation in order to have enough state to fake a transfer.
  const GURL url1("http://foo");
  const GURL url2("http://bar");
  const GURL url3("http://foo/2");

  contents()->NavigateAndCommit(url1);
  auto navigation =
      NavigationSimulator::CreateBrowserInitiated(url2, contents());
  navigation->Start();

  NavigationControllerImpl& controller = controller_impl();

  auto navigation2 =
      NavigationSimulatorImpl::CreateBrowserInitiated(url3, contents());
  NavigationController::LoadURLParams load_url_params(url3);
  load_url_params.initiator_origin = url::Origin::Create(url1);
  load_url_params.referrer = Referrer(GURL("http://referrer"),
                                      network::mojom::ReferrerPolicy::kDefault);
  load_url_params.transition_type = ui::PAGE_TRANSITION_GENERATED;
  load_url_params.extra_headers = "content-type: text/plain;\nX-Foo: Bar";
  load_url_params.load_type = NavigationController::LOAD_TYPE_DEFAULT;
  load_url_params.is_renderer_initiated = true;
  load_url_params.override_user_agent = NavigationController::UA_OVERRIDE_TRUE;
  navigation2->SetLoadURLParams(&load_url_params);
  navigation2->Start();

  NavigationEntryImpl* entry = controller.GetPendingEntry();

  // The timestamp should not have been set yet.
  ASSERT_TRUE(entry);
  EXPECT_TRUE(entry->GetTimestamp().is_null());

  CheckNavigationEntryMatchLoadParams(load_url_params, entry);
}

TEST_F(NavigationControllerTest, LoadURLWithParams_Reload) {
  NavigationControllerImpl& controller = controller_impl();
  GURL url("https://reload");

  auto navigation =
      NavigationSimulatorImpl::CreateBrowserInitiated(url, contents());
  NavigationController::LoadURLParams load_url_params(url);
  load_url_params.initiator_origin = url::Origin::Create(url);
  load_url_params.referrer = Referrer(GURL("http://referrer"),
                                      network::mojom::ReferrerPolicy::kDefault);
  load_url_params.transition_type = ui::PAGE_TRANSITION_GENERATED;
  load_url_params.extra_headers = "content-type: text/plain;\nX-Foo: Bar";
  load_url_params.load_type = NavigationController::LOAD_TYPE_DEFAULT;
  load_url_params.is_renderer_initiated = true;
  load_url_params.override_user_agent = NavigationController::UA_OVERRIDE_TRUE;
  load_url_params.reload_type = ReloadType::BYPASSING_CACHE;
  navigation->SetLoadURLParams(&load_url_params);
  navigation->Start();

  NavigationEntryImpl* entry = controller.GetPendingEntry();
  CheckNavigationEntryMatchLoadParams(load_url_params, entry);
}

TEST_F(NavigationControllerTest, LoadURLWithExtraParams_Data) {
  NavigationControllerImpl& controller = controller_impl();
  GURL url("data:text/html,dataurl");

  auto navigation =
      NavigationSimulatorImpl::CreateBrowserInitiated(url, contents());
  NavigationController::LoadURLParams load_url_params(url);
  load_url_params.load_type = NavigationController::LOAD_TYPE_DATA;
  load_url_params.base_url_for_data_url = GURL("http://foo");
  load_url_params.virtual_url_for_special_cases = GURL(url::kAboutBlankURL);
  load_url_params.override_user_agent = NavigationController::UA_OVERRIDE_FALSE;
  navigation->SetLoadURLParams(&load_url_params);
  navigation->Start();

  NavigationEntryImpl* entry = controller.GetPendingEntry();
  CheckNavigationEntryMatchLoadParams(load_url_params, entry);
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(NavigationControllerTest, LoadURLWithExtraParams_Data_Android) {
  NavigationControllerImpl& controller = controller_impl();
  GURL url("data:,");

  auto navigation =
      NavigationSimulatorImpl::CreateBrowserInitiated(url, contents());
  NavigationController::LoadURLParams load_url_params(url);
  load_url_params.load_type = NavigationController::LOAD_TYPE_DATA;
  load_url_params.base_url_for_data_url = GURL("http://foo");
  load_url_params.virtual_url_for_special_cases = GURL(url::kAboutBlankURL);
  load_url_params.data_url_as_string =
      base::MakeRefCounted<base::RefCountedString>(std::string("data:,data"));
  load_url_params.override_user_agent = NavigationController::UA_OVERRIDE_FALSE;
  navigation->SetLoadURLParams(&load_url_params);
  navigation->Start();

  NavigationEntryImpl* entry = controller.GetPendingEntry();
  CheckNavigationEntryMatchLoadParams(load_url_params, entry);
}

TEST_F(NavigationControllerTest, LoadURLWithExtraParams_Pdf_Android) {
  NavigationControllerImpl& controller = controller_impl();
  GURL url("chrome-native://pdf/link?url=https%3A%2F%2Ffoo");

  auto navigation =
      NavigationSimulatorImpl::CreateBrowserInitiated(url, contents());
  NavigationController::LoadURLParams load_url_params(url);
  load_url_params.load_type = NavigationController::LOAD_TYPE_PDF_ANDROID;
  load_url_params.virtual_url_for_special_cases = GURL("https://foo");
  load_url_params.override_user_agent = NavigationController::UA_OVERRIDE_FALSE;
  navigation->SetLoadURLParams(&load_url_params);
  navigation->Start();

  NavigationEntryImpl* entry = controller.GetPendingEntry();
  CheckNavigationEntryMatchLoadParams(load_url_params, entry);
}
#endif

TEST_F(NavigationControllerTest, KeepReloadTypeWhenCancelRepost) {
  NavigationControllerImpl& controller = controller_impl();
  GURL url("https://posturl");

  auto navigation =
      NavigationSimulatorImpl::CreateBrowserInitiated(url, contents());
  NavigationController::LoadURLParams load_url_params(url);
  load_url_params.transition_type = ui::PAGE_TRANSITION_TYPED;
  load_url_params.load_type = NavigationController::LOAD_TYPE_HTTP_POST;
  const char* raw_data = "post\n\n\0data";
  const int length = 11;
  const int64_t identifier = 1;
  load_url_params.post_data =
      network::ResourceRequestBody::CreateFromBytes(raw_data, length);
  load_url_params.post_data->set_identifier(identifier);
  navigation->SetLoadURLParams(&load_url_params);
  navigation->Start();

  // Set the post_id according to identifier.
  navigation->set_post_id(identifier);
  navigation->Commit();

  NavigationEntryImpl* entry = controller.GetLastCommittedEntry();
  ReloadType initial_reload_type = entry->reload_type();
  EXPECT_EQ(initial_reload_type, ReloadType::NONE);

  controller.Reload(ReloadType::BYPASSING_CACHE, true);
  EXPECT_EQ(1U, form_repost_counter_);
  EXPECT_EQ(entry->reload_type(), initial_reload_type);
  controller.CancelPendingReload();
  // ReloadType should not change because we canceled pending reload.
  EXPECT_EQ(entry->reload_type(), initial_reload_type);
}

TEST_F(NavigationControllerTest, LoadURLWithExtraParams_HttpPost) {
  NavigationControllerImpl& controller = controller_impl();
  GURL url("https://posturl");

  auto navigation =
      NavigationSimulatorImpl::CreateBrowserInitiated(url, contents());
  NavigationController::LoadURLParams load_url_params(url);
  load_url_params.transition_type = ui::PAGE_TRANSITION_TYPED;
  load_url_params.load_type = NavigationController::LOAD_TYPE_HTTP_POST;
  load_url_params.override_user_agent = NavigationController::UA_OVERRIDE_TRUE;
  const char* raw_data = "d\n\0a2";
  const int length = 5;
  load_url_params.post_data =
      network::ResourceRequestBody::CreateFromBytes(raw_data, length);
  navigation->SetLoadURLParams(&load_url_params);
  navigation->Start();

  NavigationEntryImpl* entry = controller.GetPendingEntry();
  CheckNavigationEntryMatchLoadParams(load_url_params, entry);
}

// Tests what happens when the same page is loaded again.  Should not create a
// new session history entry. This is what happens when you press enter in the
// URL bar to reload: a pending entry is created and then it is discarded when
// the load commits (because WebCore didn't actually make a new entry).
TEST_F(NavigationControllerTest, LoadURL_SamePage) {
  NavigationControllerImpl& controller = controller_impl();

  const GURL url1("http://foo1");

  auto navigation1 =
      NavigationSimulator::CreateBrowserInitiated(url1, contents());
  navigation1->Start();
  EXPECT_EQ(0U, navigation_entry_changed_counter_);
  EXPECT_EQ(0U, navigation_list_pruned_counter_);
  navigation1->Commit();
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;

  ASSERT_TRUE(controller.GetVisibleEntry());
  const base::Time timestamp = controller.GetVisibleEntry()->GetTimestamp();
  EXPECT_FALSE(timestamp.is_null());

  const std::string new_extra_headers("Foo: Bar\nBar: Baz");
  controller.LoadURL(url1, Referrer(), ui::PAGE_TRANSITION_TYPED,
                     new_extra_headers);
  auto navigation2 =
      NavigationSimulator::CreateFromPending(contents()->GetController());
  EXPECT_EQ(0U, navigation_entry_changed_counter_);
  EXPECT_EQ(0U, navigation_list_pruned_counter_);
  navigation2->Commit();
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;

  // We should not have produced a new session history entry.
  EXPECT_EQ(controller.GetEntryCount(), 1);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), 0);
  EXPECT_EQ(controller.GetPendingEntryIndex(), -1);
  EXPECT_TRUE(controller.GetLastCommittedEntry());
  EXPECT_FALSE(controller.GetPendingEntry());
  ASSERT_TRUE(controller.GetVisibleEntry());
  EXPECT_FALSE(controller.CanGoBack());
  EXPECT_FALSE(controller.CanGoForward());

  // The extra headers should have been updated.
  std::string new_extra_headers_crlf;
  base::ReplaceChars(new_extra_headers, "\n", "\r\n", &new_extra_headers_crlf);
  EXPECT_EQ(new_extra_headers_crlf,
            controller.GetVisibleEntry()->extra_headers());

  // The timestamp should have been updated.
  //
  // TODO(akalin): Change this EXPECT_GE (and other similar ones) to
  // EXPECT_GT once we guarantee that timestamps are unique.
  EXPECT_GE(controller.GetVisibleEntry()->GetTimestamp(), timestamp);
}

// Tests loading a URL but discarding it before the load commits.
TEST_F(NavigationControllerTest, LoadURL_Discarded) {
  NavigationControllerImpl& controller = controller_impl();

  const GURL url1("http://foo1");
  const GURL url2("http://foo2");

  auto navigation =
      NavigationSimulator::CreateBrowserInitiated(url1, contents());
  navigation->Start();
  EXPECT_EQ(0U, navigation_entry_changed_counter_);
  EXPECT_EQ(0U, navigation_list_pruned_counter_);
  navigation->Commit();
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;

  ASSERT_TRUE(controller.GetVisibleEntry());
  const base::Time timestamp = controller.GetVisibleEntry()->GetTimestamp();
  EXPECT_FALSE(timestamp.is_null());

  controller.LoadURL(url2, Referrer(), ui::PAGE_TRANSITION_TYPED,
                     std::string());
  controller.DiscardNonCommittedEntries();
  EXPECT_EQ(0U, navigation_entry_changed_counter_);
  EXPECT_EQ(0U, navigation_list_pruned_counter_);

  // Should not have produced a new session history entry.
  EXPECT_EQ(controller.GetEntryCount(), 1);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), 0);
  EXPECT_EQ(controller.GetPendingEntryIndex(), -1);
  EXPECT_TRUE(controller.GetLastCommittedEntry());
  EXPECT_FALSE(controller.GetPendingEntry());
  ASSERT_TRUE(controller.GetVisibleEntry());
  EXPECT_FALSE(controller.CanGoBack());
  EXPECT_FALSE(controller.CanGoForward());

  // Timestamp should not have changed.
  EXPECT_EQ(timestamp, controller.GetVisibleEntry()->GetTimestamp());
}

// Tests navigations that come in unrequested. This happens when the user
// navigates from the web page, and here we test that there is no pending entry.
TEST_F(NavigationControllerTest, LoadURL_NoPending) {
  NavigationControllerImpl& controller = controller_impl();

  // First make an existing committed entry.
  const GURL kExistingURL1("http://eh");
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), kExistingURL1);
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;

  // Do a new navigation without making a pending one.
  const GURL kNewURL("http://see");
  NavigationSimulator::NavigateAndCommitFromDocument(kNewURL, main_test_rfh());

  // There should no longer be any pending entry, and the second navigation we
  // just made should be committed.
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;
  EXPECT_EQ(-1, controller.GetPendingEntryIndex());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(kNewURL, controller.GetVisibleEntry()->GetURL());
}

// Tests navigating to a new URL when there is a new pending navigation that is
// not the one that just loaded. This will happen if the user types in a URL to
// somewhere slow, and then navigates the current page before the typed URL
// commits.
TEST_F(NavigationControllerTest, LoadURL_NewPending) {
  NavigationControllerImpl& controller = controller_impl();

  // First make an existing committed entry.
  const GURL kExistingURL1("http://eh");
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), kExistingURL1);
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;

  // Make a pending entry to somewhere new.
  const GURL kExistingURL2("http://bee");
  auto navigation =
      NavigationSimulator::CreateBrowserInitiated(kExistingURL2, contents());
  navigation->Start();
  EXPECT_EQ(0U, navigation_entry_changed_counter_);
  EXPECT_EQ(0U, navigation_list_pruned_counter_);

  // After the navigation starts but before it commits, do a new navigation.
  const GURL kNewURL("http://see");
  NavigationSimulator::NavigateAndCommitFromDocument(kNewURL, main_test_rfh());

  // There should no longer be any pending entry, and the third navigation we
  // just made should be committed.
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;
  EXPECT_EQ(-1, controller.GetPendingEntryIndex());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(kNewURL, controller.GetVisibleEntry()->GetURL());
}

// Tests navigating to a new URL when there is a pending back/forward
// navigation. This will happen if the user hits back, but before that commits,
// they navigate somewhere new.
TEST_F(NavigationControllerTest, LoadURL_ExistingPending) {
  NavigationControllerImpl& controller = controller_impl();

  // First make some history.
  const GURL kExistingURL1("http://foo/eh");
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), kExistingURL1);
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;

  // Simulate a user gesture so that the above entry is not marked to be skipped
  // on back.
  main_test_rfh()->frame_tree_node()->UpdateUserActivationState(
      blink::mojom::UserActivationUpdateType::kNotifyActivation,
      blink::mojom::UserActivationNotificationType::kTest);

  const GURL kExistingURL2("http://foo/bee");
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), kExistingURL2);
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;

  // Now make a pending back/forward navigation. The zeroth entry should be
  // pending.
  controller.GoBack();
  EXPECT_EQ(0U, navigation_entry_changed_counter_);
  EXPECT_EQ(0U, navigation_list_pruned_counter_);
  EXPECT_EQ(0, controller.GetPendingEntryIndex());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());

  // Before that commits, do a new navigation.
  const GURL kNewURL("http://foo/see");
  NavigationSimulator::NavigateAndCommitFromDocument(kNewURL, main_test_rfh());

  // There should no longer be any pending entry, and the new navigation we
  // just made should be committed.
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;
  EXPECT_EQ(-1, controller.GetPendingEntryIndex());
  EXPECT_EQ(2, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(kNewURL, controller.GetVisibleEntry()->GetURL());
}

// Tests navigating to a new URL when there is a pending back/forward
// navigation to a cross-process, privileged URL. This will happen if the user
// hits back, but before that commits, they navigate somewhere new.
TEST_F(NavigationControllerTest, LoadURL_PrivilegedPending) {
  NavigationControllerImpl& controller = controller_impl();

  // First make some history, starting with a privileged URL.
  const GURL kExistingURL1("chrome://gpu");
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), kExistingURL1);
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;
  EXPECT_EQ(BindingsPolicySet({BindingsPolicyValue::kWebUi}),
            controller.GetLastCommittedEntry()
                ->GetFrameEntry(root_ftn())
                ->bindings());
  // Simulate a user gesture so that the above entry is not marked to be skipped
  // on back.
  main_test_rfh()->frame_tree_node()->UpdateUserActivationState(
      blink::mojom::UserActivationUpdateType::kNotifyActivation,
      blink::mojom::UserActivationNotificationType::kTest);

  // Navigate cross-process to a second URL.
  const GURL kExistingURL2("http://foo/eh");
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), kExistingURL2);
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;
  EXPECT_TRUE(controller.GetLastCommittedEntry()
                  ->GetFrameEntry(root_ftn())
                  ->bindings()
                  ->empty());

  // Now make a pending back/forward navigation to a privileged entry.
  // The zeroth entry should be pending.
  auto back_navigation = NavigationSimulator::CreateHistoryNavigation(
      -1, contents(), false /* is_renderer_initiated */);
  back_navigation->Start();
  EXPECT_EQ(0U, navigation_entry_changed_counter_);
  EXPECT_EQ(0U, navigation_list_pruned_counter_);
  EXPECT_EQ(0, controller.GetPendingEntryIndex());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(
      BindingsPolicySet({BindingsPolicyValue::kWebUi}),
      controller.GetPendingEntry()->GetFrameEntry(root_ftn())->bindings());

  // Before that commits, do a new navigation.
  const GURL kNewURL("http://foo/bee");
  NavigationSimulator::NavigateAndCommitFromDocument(kNewURL, main_test_rfh());

  // There should no longer be any pending entry, and the new navigation we
  // just made should be committed.
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;
  EXPECT_EQ(-1, controller.GetPendingEntryIndex());
  EXPECT_EQ(2, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(kNewURL, controller.GetVisibleEntry()->GetURL());
  EXPECT_TRUE(controller.GetLastCommittedEntry()
                  ->GetFrameEntry(root_ftn())
                  ->bindings()
                  ->empty());
}

// Tests navigating to an existing URL when there is a pending new navigation.
// This will happen if the user enters a URL, but before that commits, the
// current page fires history.back().
TEST_F(NavigationControllerTest, LoadURL_BackPreemptsPending) {
  NavigationControllerImpl& controller = controller_impl();
  // The test assumes the previous page gets deleted after navigation. Disable
  // back/forward cache to ensure that it doesn't get preserved in the cache.
  DisableBackForwardCacheForTesting(RenderViewHostTestHarness::web_contents(),
                                    BackForwardCache::TEST_REQUIRES_NO_CACHING);

  // First make some history.
  const GURL kExistingURL1("http://foo/eh");
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), kExistingURL1);
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;

  const GURL kExistingURL2("http://foo/bee");
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), kExistingURL2);
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;

  // A back navigation comes in from the renderer...
  auto back_navigation = NavigationSimulator::CreateHistoryNavigation(
      -1, contents(), false /* is_renderer_initiated */);
  back_navigation->ReadyToCommit();

  // ...while the user tries to navigate to a new page...
  const GURL kNewURL("http://foo/see");
  auto new_navigation =
      NavigationSimulator::CreateBrowserInitiated(kNewURL, contents());
  new_navigation->Start();
  EXPECT_EQ(0U, navigation_entry_changed_counter_);
  EXPECT_EQ(0U, navigation_list_pruned_counter_);
  EXPECT_EQ(-1, controller.GetPendingEntryIndex());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());

  // ...and the back navigation commits.
  back_navigation->Commit();

  // There should no longer be any pending entry, and the back navigation should
  // be committed.
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;
  EXPECT_EQ(-1, controller.GetPendingEntryIndex());
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(kExistingURL1, controller.GetVisibleEntry()->GetURL());
}

// Verify that a direct commit message from the renderer properly cancels a
// pending new navigation. This will happen if the user enters a URL, but
// before that commits, the current blank page reloads.
// Original bug: http://crbug.com/77507.
TEST_F(NavigationControllerTest, LoadURL_IgnorePreemptsPending) {
  NavigationControllerImpl& controller = controller_impl();

  // Set a WebContentsDelegate to listen for state changes.
  std::unique_ptr<TestWebContentsDelegate> delegate(
      new TestWebContentsDelegate());
  EXPECT_FALSE(contents()->GetDelegate());
  contents()->SetDelegate(delegate.get());

  // Without any navigations, the renderer starts at about:blank.
  const GURL kExistingURL(url::kAboutBlankURL);

  // Now make a pending new navigation.
  const GURL kNewURL("http://eh");
  auto navigation =
      NavigationSimulator::CreateBrowserInitiated(kNewURL, contents());
  navigation->Start();
  EXPECT_EQ(0U, navigation_entry_changed_counter_);
  EXPECT_EQ(0U, navigation_list_pruned_counter_);
  EXPECT_EQ(-1, controller.GetPendingEntryIndex());
  EXPECT_TRUE(controller.GetPendingEntry());
  EXPECT_TRUE(!controller.GetLastCommittedEntry() ||
              controller.GetLastCommittedEntry()->IsInitialEntry());
  EXPECT_EQ(1, delegate->navigation_state_change_count());

  // Certain rare cases can make a direct DidCommitProvisionalLoad call without
  // going to the browser. Renderer reload of an about:blank is such a case.
  main_test_rfh()->SendNavigate(0, false, kExistingURL);

  // This should clear the pending entry and notify of a navigation state
  // change, so that we do not keep displaying kNewURL.
  EXPECT_EQ(-1, controller.GetPendingEntryIndex());
  EXPECT_FALSE(controller.GetPendingEntry());
  // The pending entry deletion and commit of the new NavigationEntry both
  // counts as "navigation state change".
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(3, delegate->navigation_state_change_count());

  contents()->SetDelegate(nullptr);
}

// Tests that the pending entry state is correct after an abort.
// We do not want to clear the pending entry, so that the user doesn't
// lose a typed URL.  (See http://crbug.com/9682.)
TEST_F(NavigationControllerTest, LoadURL_AbortDoesntCancelPending) {
  NavigationControllerImpl& controller = controller_impl();

  // Set a WebContentsDelegate to listen for state changes.
  std::unique_ptr<TestWebContentsDelegate> delegate(
      new TestWebContentsDelegate());
  EXPECT_FALSE(contents()->GetDelegate());
  contents()->SetDelegate(delegate.get());

  // Start with a pending new navigation.
  const GURL kNewURL("http://eh");
  auto navigation =
      NavigationSimulator::CreateBrowserInitiated(kNewURL, contents());
  navigation->Start();
  EXPECT_EQ(0U, navigation_entry_changed_counter_);
  EXPECT_EQ(0U, navigation_list_pruned_counter_);
  EXPECT_EQ(-1, controller.GetPendingEntryIndex());
  EXPECT_TRUE(controller.GetPendingEntry());
  EXPECT_EQ(kNewURL, controller.GetPendingEntry()->GetURL());
  EXPECT_TRUE(controller.GetLastCommittedEntry()->IsInitialEntry());
  EXPECT_EQ(1, delegate->navigation_state_change_count());

  // It may abort before committing, if it's a download or due to a stop or
  // a new navigation from the user.
  navigation->AbortCommit();

  // This should not clear the pending entry, so that we keep displaying
  // kNewURL (until the user clears it).
  EXPECT_EQ(-1, controller.GetPendingEntryIndex());
  EXPECT_TRUE(controller.GetPendingEntry());
  EXPECT_EQ(kNewURL, controller.GetPendingEntry()->GetURL());
  EXPECT_TRUE(controller.GetLastCommittedEntry()->IsInitialEntry());
  EXPECT_EQ(2, delegate->navigation_state_change_count());
  NavigationEntry* pending_entry = controller.GetPendingEntry();

  // Ensure that a reload keeps the same pending entry.
  controller.Reload(ReloadType::NORMAL, true);
  EXPECT_EQ(-1, controller.GetPendingEntryIndex());
  EXPECT_TRUE(controller.GetPendingEntry());
  EXPECT_EQ(kNewURL, controller.GetPendingEntry()->GetURL());
  EXPECT_EQ(pending_entry, controller.GetPendingEntry());
  EXPECT_TRUE(controller.GetLastCommittedEntry()->IsInitialEntry());

  contents()->SetDelegate(nullptr);
}

// Tests that the pending URL is not visible during a renderer-initiated
// redirect and abort.  See http://crbug.com/83031.
TEST_F(NavigationControllerTest, LoadURL_RedirectAbortDoesntShowPendingURL) {
  NavigationControllerImpl& controller = controller_impl();

  // First make an existing committed entry.
  const GURL kExistingURL("http://foo/eh");
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), kExistingURL);
  static_cast<mojom::FrameHost*>(main_test_rfh())->DidStopLoading();
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;

  // Set a WebContentsDelegate to listen for state changes.
  std::unique_ptr<TestWebContentsDelegate> delegate(
      new TestWebContentsDelegate());
  EXPECT_FALSE(contents()->GetDelegate());
  contents()->SetDelegate(delegate.get());

  // Now make a pending new navigation, initiated by the renderer.
  const GURL kNewURL("http://foo/bee");
  auto navigation =
      NavigationSimulator::CreateRendererInitiated(kNewURL, main_test_rfh());
  navigation->Start();
  EXPECT_EQ(0U, navigation_entry_changed_counter_);
  EXPECT_EQ(0U, navigation_list_pruned_counter_);
  EXPECT_EQ(-1, controller.GetPendingEntryIndex());
  EXPECT_TRUE(controller.GetPendingEntry());
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());
  // The delegate should have been notified twice: once for the loading state
  // change, and once for the url change.
  EXPECT_EQ(2, delegate->navigation_state_change_count());

  // The visible entry should be the last committed URL, not the pending one.
  EXPECT_EQ(kExistingURL, controller.GetVisibleEntry()->GetURL());

  // Now the navigation redirects.
  const GURL kRedirectURL("http://foo/see");
  navigation->Redirect(kRedirectURL);

  // We don't want to change the NavigationEntry's url, in case it cancels.
  // Prevents regression of http://crbug.com/77786.
  EXPECT_EQ(kNewURL, controller.GetPendingEntry()->GetURL());

  // It may abort before committing, if it's a download or due to a stop or
  // a new navigation from the user.
  navigation->Fail(net::ERR_ABORTED);

  // Because the pending entry is renderer initiated and not visible, we
  // clear it when it fails.
  EXPECT_EQ(-1, controller.GetPendingEntryIndex());
  EXPECT_FALSE(controller.GetPendingEntry());
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());
  // The delegate should have been notified twice: once for the loading state
  // change, and once for the url change.
  EXPECT_EQ(4, delegate->navigation_state_change_count());

  // The visible entry should be the last committed URL, not the pending one,
  // so that no spoof is possible.
  EXPECT_EQ(kExistingURL, controller.GetVisibleEntry()->GetURL());

  contents()->SetDelegate(nullptr);
}

TEST_F(NavigationControllerTest, Reload) {
  NavigationControllerImpl& controller = controller_impl();

  const GURL url1("http://foo1");

  auto navigation =
      NavigationSimulator::CreateBrowserInitiated(url1, contents());
  navigation->Start();
  EXPECT_EQ(0U, navigation_entry_changed_counter_);
  EXPECT_EQ(0U, navigation_list_pruned_counter_);
  navigation->Commit();
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;
  ASSERT_TRUE(controller.GetVisibleEntry());

  controller.Reload(ReloadType::NORMAL, true);
  navigation = NavigationSimulator::CreateFromPending(
      RenderViewHostTestHarness::web_contents()->GetController());
  EXPECT_EQ(0U, navigation_entry_changed_counter_);
  EXPECT_EQ(0U, navigation_list_pruned_counter_);

  const base::Time timestamp = controller.GetVisibleEntry()->GetTimestamp();
  EXPECT_FALSE(timestamp.is_null());

  // The reload is pending.
  EXPECT_EQ(controller.GetEntryCount(), 1);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), 0);
  EXPECT_EQ(controller.GetPendingEntryIndex(), 0);
  EXPECT_TRUE(controller.GetLastCommittedEntry());
  EXPECT_TRUE(controller.GetPendingEntry());
  EXPECT_FALSE(controller.CanGoBack());
  EXPECT_FALSE(controller.CanGoForward());
  // So no committed reload in RFH yet.
  EXPECT_EQ(ReloadType::NONE, main_test_rfh()->reload_type());

  navigation->Commit();
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;

  // Now the reload is committed.
  EXPECT_EQ(controller.GetEntryCount(), 1);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), 0);
  EXPECT_EQ(controller.GetPendingEntryIndex(), -1);
  EXPECT_TRUE(controller.GetLastCommittedEntry());
  EXPECT_FALSE(controller.GetPendingEntry());
  EXPECT_FALSE(controller.CanGoBack());
  EXPECT_FALSE(controller.CanGoForward());
  EXPECT_EQ(ReloadType::NORMAL, main_test_rfh()->reload_type());

  // The timestamp should have been updated.
  ASSERT_TRUE(controller.GetVisibleEntry());
  EXPECT_GE(controller.GetVisibleEntry()->GetTimestamp(), timestamp);
}

// Tests what happens when a reload navigation produces a new page.
TEST_F(NavigationControllerTest, Reload_GeneratesNewPage) {
  NavigationControllerImpl& controller = controller_impl();

  const GURL url1("http://foo1");
  const GURL url2("http://foo2");

  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), url1);
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;

  controller.Reload(ReloadType::NORMAL, true);
  EXPECT_EQ(0U, navigation_entry_changed_counter_);
  EXPECT_EQ(0U, navigation_list_pruned_counter_);

  auto reload =
      NavigationSimulator::CreateFromPending(contents()->GetController());
  reload->Redirect(url2);
  reload->Commit();
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;

  // Now the reload is committed.
  EXPECT_EQ(controller.GetEntryCount(), 1);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), 0);
  EXPECT_EQ(controller.GetPendingEntryIndex(), -1);
  EXPECT_TRUE(controller.GetLastCommittedEntry());
  EXPECT_FALSE(controller.GetPendingEntry());
  EXPECT_FALSE(controller.CanGoBack());
  EXPECT_FALSE(controller.CanGoForward());
}

// This test ensures that when a guest renderer reloads, the reload goes through
// without ending up in the "we have a wrong process for the URL" branch in
// NavigationControllerImpl::ReloadInternal.
TEST_F(NavigationControllerTest, ReloadWithGuest) {
  const StoragePartitionConfig kGuestPartitionConfig =
      StoragePartitionConfig::Create(browser_context(), "someapp",
                                     "somepartition", /*in_memory=*/false);
  scoped_refptr<SiteInstance> guest_instance =
      SiteInstance::CreateForGuest(browser_context(), kGuestPartitionConfig);
  std::unique_ptr<TestWebContents> guest_web_contents(
      TestWebContents::Create(browser_context(), guest_instance));
  NavigationControllerImpl& controller = guest_web_contents->GetController();

  const GURL url1("http://foo1");
  NavigationSimulator::NavigateAndCommitFromBrowser(guest_web_contents.get(),
                                                    url1);
  ASSERT_TRUE(controller.GetVisibleEntry());

  // Ensure the entry's SiteInstance and RenderProcessHost are for a guest.
  NavigationEntryImpl* entry1 = controller.GetVisibleEntry();
  ASSERT_EQ(entry1->site_instance()->GetStoragePartitionConfig(),
            kGuestPartitionConfig);
  ASSERT_TRUE(entry1->site_instance()->IsGuest());
  ASSERT_TRUE(entry1->site_instance()->GetProcess()->IsForGuestsOnly());

  // And reload.
  controller.Reload(ReloadType::NORMAL, true);

  // The reload is pending. Check that the NavigationEntry didn't get replaced
  // because of having the wrong process.
  EXPECT_EQ(controller.GetEntryCount(), 1);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), 0);
  EXPECT_EQ(controller.GetPendingEntryIndex(), 0);

  NavigationEntryImpl* entry2 = controller.GetPendingEntry();
  EXPECT_EQ(entry1, entry2);
}

TEST_F(NavigationControllerTest, ReloadOriginalRequestURL) {
  NavigationControllerImpl& controller = controller_impl();

  const GURL original_url("http://foo1");
  const GURL final_url("http://foo2");

  // Load up the original URL, but get redirected.
  auto navigation =
      NavigationSimulator::CreateBrowserInitiated(original_url, contents());
  navigation->Start();
  EXPECT_EQ(0U, navigation_entry_changed_counter_);
  EXPECT_EQ(0U, navigation_list_pruned_counter_);
  navigation->Redirect(final_url);
  navigation->Commit();
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;
  EXPECT_EQ(ReloadType::NONE, main_test_rfh()->reload_type());

  // The NavigationEntry should save both the original URL and the final
  // redirected URL.
  EXPECT_EQ(original_url,
            controller.GetVisibleEntry()->GetOriginalRequestURL());
  EXPECT_EQ(final_url, controller.GetVisibleEntry()->GetURL());

  // Reload using the original URL.
  controller.LoadOriginalRequestURL();
  EXPECT_EQ(0U, navigation_entry_changed_counter_);
  EXPECT_EQ(0U, navigation_list_pruned_counter_);

  // The reload is pending as a new pending entry with replacement. The request
  // should point to the original URL.
  EXPECT_EQ(original_url, navigated_url());
  EXPECT_EQ(controller.GetEntryCount(), 1);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), 0);
  EXPECT_EQ(controller.GetPendingEntryIndex(), -1);
  EXPECT_TRUE(controller.GetLastCommittedEntry());
  EXPECT_TRUE(controller.GetPendingEntry());
  EXPECT_FALSE(controller.CanGoBack());
  EXPECT_FALSE(controller.CanGoForward());

  // Send that the navigation has proceeded; say it got redirected again.
  navigation = NavigationSimulator::CreateFromPending(
      RenderViewHostTestHarness::web_contents()->GetController());
  navigation->Redirect(final_url);
  navigation->Commit();
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;

  // Now the reload is committed.
  EXPECT_EQ(controller.GetEntryCount(), 1);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), 0);
  EXPECT_EQ(controller.GetPendingEntryIndex(), -1);
  EXPECT_TRUE(controller.GetLastCommittedEntry());
  EXPECT_FALSE(controller.GetPendingEntry());
  EXPECT_FALSE(controller.CanGoBack());
  EXPECT_FALSE(controller.CanGoForward());
  EXPECT_EQ(ReloadType::NONE, main_test_rfh()->reload_type());
}

// Test that certain non-persisted NavigationEntryImpl values get reset after
// commit.
TEST_F(NavigationControllerTest, ResetEntryValuesAfterCommit) {
  NavigationControllerImpl& controller = controller_impl();

  // The value of "should replace entry" will be tested, but it's an error to
  // specify it when there are no entries. Create a simple entry to be replaced.
  const GURL url0("http://foo/0");
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), url0);

  // Set up the pending entry.
  const GURL url1("http://foo/1");
  auto navigation =
      NavigationSimulatorImpl::CreateBrowserInitiated(url1, contents());
  navigation->set_should_replace_current_entry(true);
  navigation->Start();

  // Set up some sample values.
  const char* raw_data = "post\n\n\0data";
  const int length = 11;

  // Set non-persisted values on the pending entry.
  NavigationEntryImpl* pending_entry = controller.GetPendingEntry();
  pending_entry->SetPostData(
      network::ResourceRequestBody::CreateFromBytes(raw_data, length));
  pending_entry->set_is_renderer_initiated(true);
  pending_entry->set_should_clear_history_list(true);
  EXPECT_TRUE(pending_entry->GetPostData());
  EXPECT_TRUE(pending_entry->is_renderer_initiated());
  EXPECT_TRUE(pending_entry->should_clear_history_list());

  // Fake a commit response.
  navigation->Commit();

  // Certain values that are only used for pending entries get reset after
  // commit.
  NavigationEntryImpl* committed_entry = controller.GetLastCommittedEntry();
  EXPECT_FALSE(committed_entry->GetPostData());
  EXPECT_FALSE(committed_entry->is_renderer_initiated());
  EXPECT_FALSE(committed_entry->should_clear_history_list());
}

// Test that Redirects are preserved after a commit.
TEST_F(NavigationControllerTest, RedirectsAreNotResetByCommit) {
  NavigationControllerImpl& controller = controller_impl();
  const GURL url1("http://foo1");
  const GURL url2("http://foo2");
  auto navigation =
      NavigationSimulator::CreateBrowserInitiated(url1, contents());
  navigation->Start();

  // Set up some redirect values.
  std::vector<GURL> redirects;
  redirects.push_back(url2);

  // Set redirects on the pending entry.
  NavigationEntryImpl* pending_entry = controller.GetPendingEntry();
  pending_entry->SetRedirectChain(redirects);
  EXPECT_EQ(1U, pending_entry->GetRedirectChain().size());
  EXPECT_EQ(url2, pending_entry->GetRedirectChain()[0]);

  // Normal navigation will preserve redirects in the committed entry.
  navigation->Redirect(url2);
  navigation->Commit();
  NavigationEntryImpl* committed_entry = controller.GetLastCommittedEntry();

  // The navigation started out trying to get to |url1|, but got redirected to
  // |url2|, so they're both in the redirect chain.
  ASSERT_EQ(2U, committed_entry->GetRedirectChain().size());
  EXPECT_EQ(url1, committed_entry->GetRedirectChain()[0]);
  EXPECT_EQ(url2, committed_entry->GetRedirectChain()[1]);
}

// Tests that webkit preferences are updated when user agent override changes.
TEST_F(NavigationControllerTest, GoBackWithUserAgentOverrideChange) {
  // The test requires that going back will load a new document instead of
  // restoring an old one from the back/forward cache.
  DisableBackForwardCacheForTesting(RenderViewHostTestHarness::web_contents(),
                                    BackForwardCache::TEST_REQUIRES_NO_CACHING);
  NavigationControllerImpl& controller = controller_impl();

  // Set up a simple NavigationEntry stack of two pages.
  const GURL url1("http://foo/1");
  const GURL url2("http://foo/2");

  auto navigation1 =
      NavigationSimulator::CreateBrowserInitiated(url1, contents());
  navigation1->Start();
  EXPECT_FALSE(controller.GetPendingEntry()->GetIsOverridingUserAgent());
  navigation1->Commit();

  auto navigation2 =
      NavigationSimulator::CreateBrowserInitiated(url2, contents());
  navigation2->Start();
  EXPECT_FALSE(controller.GetPendingEntry()->GetIsOverridingUserAgent());
  navigation2->Commit();

  // Simulate the behavior of "Request Desktop Site" being checked in
  // NavigationControllerAndroid::SetUseDesktopUserAgent.
  controller.GetVisibleEntry()->SetIsOverridingUserAgent(true);
  controller.LoadOriginalRequestURL();
  auto reload =
      NavigationSimulator::CreateFromPending(contents()->GetController());
  reload->Commit();
  EXPECT_TRUE(controller.GetLastCommittedEntry()->GetIsOverridingUserAgent());

  // Test that OnWebkitPreferencesChanged is called when going back to propagate
  // change in viewport_meta WebSetting.
  int change_counter = 0;
  contents()->set_web_preferences_changed_counter(&change_counter);

  auto back_navigation = NavigationSimulator::CreateHistoryNavigation(
      -1, contents(), false /* is_renderer_initiated */);
  back_navigation->Start();
  EXPECT_FALSE(controller.GetPendingEntry()->GetIsOverridingUserAgent());
  back_navigation->Commit();

  EXPECT_EQ(1, change_counter);
  contents()->set_web_preferences_changed_counter(nullptr);
}

// Tests what happens when we navigate back successfully
TEST_F(NavigationControllerTest, Back) {
  NavigationControllerImpl& controller = controller_impl();

  const GURL url1("http://foo1");
  NavigationSimulator::NavigateAndCommitFromDocument(url1, main_test_rfh());
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;

  const GURL url2("http://foo2");
  NavigationSimulator::NavigateAndCommitFromDocument(url2, main_test_rfh());
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;

  auto back_navigation = NavigationSimulator::CreateHistoryNavigation(
      -1, contents(), false /* is_renderer_initiated */);
  back_navigation->Start();
  EXPECT_EQ(0U, navigation_entry_changed_counter_);
  EXPECT_EQ(0U, navigation_list_pruned_counter_);

  // We should now have a pending navigation to go back.
  EXPECT_EQ(controller.GetEntryCount(), 2);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), 1);
  EXPECT_EQ(controller.GetPendingEntryIndex(), 0);
  EXPECT_TRUE(controller.GetLastCommittedEntry());
  EXPECT_TRUE(controller.GetPendingEntry());
  EXPECT_FALSE(controller.CanGoBack());
  EXPECT_FALSE(controller.CanGoToOffset(-1));
  EXPECT_TRUE(controller.CanGoForward());
  EXPECT_TRUE(controller.CanGoToOffset(1));
  EXPECT_FALSE(controller.CanGoToOffset(2));  // Cannot go forward 2 steps.

  // Timestamp for entry 1 should be on or after that of entry 0.
  EXPECT_FALSE(controller.GetEntryAtIndex(0)->GetTimestamp().is_null());
  EXPECT_GE(controller.GetEntryAtIndex(1)->GetTimestamp(),
            controller.GetEntryAtIndex(0)->GetTimestamp());

  back_navigation->Commit();
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;

  // The back navigation completed successfully.
  EXPECT_EQ(controller.GetEntryCount(), 2);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), 0);
  EXPECT_EQ(controller.GetPendingEntryIndex(), -1);
  EXPECT_TRUE(controller.GetLastCommittedEntry());
  EXPECT_FALSE(controller.GetPendingEntry());
  EXPECT_FALSE(controller.CanGoBack());
  EXPECT_FALSE(controller.CanGoToOffset(-1));
  EXPECT_TRUE(controller.CanGoForward());
  EXPECT_TRUE(controller.CanGoToOffset(1));
  EXPECT_FALSE(controller.CanGoToOffset(2));  // Cannot go foward 2 steps.

  // Timestamp for entry 0 should be on or after that of entry 1
  // (since we went back to it).
  EXPECT_GE(controller.GetEntryAtIndex(0)->GetTimestamp(),
            controller.GetEntryAtIndex(1)->GetTimestamp());
}

// Tests what happens when a back navigation produces a new page.
TEST_F(NavigationControllerTest, Back_GeneratesNewPage) {
  NavigationControllerImpl& controller = controller_impl();
  // The test assumes the previous page gets deleted after navigation. Disable
  // back/forward cache to ensure that it doesn't get preserved in the cache.
  DisableBackForwardCacheForTesting(RenderViewHostTestHarness::web_contents(),
                                    BackForwardCache::TEST_REQUIRES_NO_CACHING);

  const GURL url1("http://foo/1");
  const GURL url2("http://foo/2");
  const GURL url3("http://foo/3");

  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), url1);
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;

  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), url2);
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;

  auto navigation = NavigationSimulator::CreateHistoryNavigation(
      -1, contents(), false /* is_renderer_initiated */);
  navigation->Start();
  EXPECT_EQ(0U, navigation_entry_changed_counter_);
  EXPECT_EQ(0U, navigation_list_pruned_counter_);

  // We should now have a pending navigation to go back.
  EXPECT_EQ(controller.GetEntryCount(), 2);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), 1);
  EXPECT_EQ(controller.GetPendingEntryIndex(), 0);
  EXPECT_TRUE(controller.GetLastCommittedEntry());
  EXPECT_TRUE(controller.GetPendingEntry());
  EXPECT_FALSE(controller.CanGoBack());
  EXPECT_TRUE(controller.CanGoForward());

  navigation->Redirect(url3);
  navigation->Commit();
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;

  // The first NavigationEntry should have been used.
  EXPECT_EQ(controller.GetEntryCount(), 2);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), 0);
  EXPECT_EQ(controller.GetPendingEntryIndex(), -1);
  EXPECT_TRUE(controller.GetLastCommittedEntry());
  EXPECT_FALSE(controller.GetPendingEntry());
  EXPECT_FALSE(controller.CanGoBack());
  EXPECT_TRUE(controller.CanGoForward());
}

// Receives a back message when there is a new pending navigation entry.
TEST_F(NavigationControllerTest, Back_NewPending) {
  NavigationControllerImpl& controller = controller_impl();

  const GURL kUrl1("http://foo1");
  const GURL kUrl2("http://foo2");
  const GURL kUrl3("http://foo3");

  // First navigate two places so we have some back history.
  NavigationSimulator::NavigateAndCommitFromDocument(kUrl1, main_test_rfh());
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;

  // Simulate a user gesture so that the above entry is not marked to be skipped
  // on back.
  main_test_rfh()->frame_tree_node()->UpdateUserActivationState(
      blink::mojom::UserActivationUpdateType::kNotifyActivation,
      blink::mojom::UserActivationNotificationType::kTest);

  // controller.LoadURL(kUrl2, ui::PAGE_TRANSITION_TYPED);
  NavigationSimulator::NavigateAndCommitFromDocument(kUrl2, main_test_rfh());
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;

  // Now start a new pending navigation and go back before it commits.
  controller.LoadURL(kUrl3, Referrer(), ui::PAGE_TRANSITION_TYPED,
                     std::string());
  EXPECT_EQ(-1, controller.GetPendingEntryIndex());
  EXPECT_EQ(kUrl3, controller.GetPendingEntry()->GetURL());
  controller.GoBack();

  // The pending navigation should now be the "back" item and the new one
  // should be gone.
  EXPECT_EQ(0, controller.GetPendingEntryIndex());
  EXPECT_EQ(kUrl1, controller.GetPendingEntry()->GetURL());
}

// Tests what happens when we navigate forward successfully.
TEST_F(NavigationControllerTest, Forward) {
  NavigationControllerImpl& controller = controller_impl();

  const GURL url1("http://foo1");
  const GURL url2("http://foo2");

  NavigationSimulator::NavigateAndCommitFromDocument(url1, main_test_rfh());
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;

  NavigationSimulator::NavigateAndCommitFromDocument(url2, main_test_rfh());
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;

  NavigationSimulator::GoBack(contents());
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;

  // Simulate a user gesture so that the above entry is not marked to be skipped
  // on back.
  main_test_rfh()->frame_tree_node()->UpdateUserActivationState(
      blink::mojom::UserActivationUpdateType::kNotifyActivation,
      blink::mojom::UserActivationNotificationType::kTest);

  auto forward_navigation = NavigationSimulator::CreateHistoryNavigation(
      1, contents(), false /* is_renderer_initiated */);
  forward_navigation->Start();
  // We should now have a pending navigation to go forward.
  EXPECT_EQ(controller.GetEntryCount(), 2);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), 0);
  EXPECT_EQ(controller.GetPendingEntryIndex(), 1);
  EXPECT_TRUE(controller.GetLastCommittedEntry());
  EXPECT_TRUE(controller.GetPendingEntry());
  EXPECT_TRUE(controller.CanGoBack());
  EXPECT_TRUE(controller.CanGoToOffset(-1));
  EXPECT_FALSE(controller.CanGoToOffset(-2));  // Cannot go back 2 steps.
  EXPECT_FALSE(controller.CanGoForward());
  EXPECT_FALSE(controller.CanGoToOffset(1));

  // Timestamp for entry 0 should be on or after that of entry 1
  // (since we went back to it).
  EXPECT_FALSE(controller.GetEntryAtIndex(0)->GetTimestamp().is_null());
  EXPECT_GE(controller.GetEntryAtIndex(0)->GetTimestamp(),
            controller.GetEntryAtIndex(1)->GetTimestamp());

  forward_navigation->Commit();
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;

  // The forward navigation completed successfully.
  EXPECT_EQ(controller.GetEntryCount(), 2);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), 1);
  EXPECT_EQ(controller.GetPendingEntryIndex(), -1);
  EXPECT_TRUE(controller.GetLastCommittedEntry());
  EXPECT_FALSE(controller.GetPendingEntry());
  EXPECT_TRUE(controller.CanGoBack());
  EXPECT_TRUE(controller.CanGoToOffset(-1));
  EXPECT_FALSE(controller.CanGoToOffset(-2));  // Cannot go back 2 steps.
  EXPECT_FALSE(controller.CanGoForward());
  EXPECT_FALSE(controller.CanGoToOffset(1));

  // Timestamp for entry 1 should be on or after that of entry 0
  // (since we went forward to it).
  EXPECT_GE(controller.GetEntryAtIndex(1)->GetTimestamp(),
            controller.GetEntryAtIndex(0)->GetTimestamp());
}

// Tests what happens when a forward navigation produces a new page.
TEST_F(NavigationControllerTest, Forward_GeneratesNewPage) {
  NavigationControllerImpl& controller = controller_impl();

  const GURL url1("http://foo1");
  const GURL url2("http://foo2");
  const GURL url3("http://foo3");

  NavigationSimulator::NavigateAndCommitFromDocument(url1, main_test_rfh());
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;

  NavigationSimulator::NavigateAndCommitFromDocument(url2, main_test_rfh());
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;

  NavigationSimulator::GoBack(contents());
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;

  // Simulate a user gesture so that the above entry is not marked to be skipped
  // on back.
  main_test_rfh()->frame_tree_node()->UpdateUserActivationState(
      blink::mojom::UserActivationUpdateType::kNotifyActivation,
      blink::mojom::UserActivationNotificationType::kTest);

  auto forward_navigation = NavigationSimulator::CreateHistoryNavigation(
      1, contents(), false /* is_renderer_initiated */);
  forward_navigation->Start();
  EXPECT_EQ(0U, navigation_list_pruned_counter_);

  // Should now have a pending navigation to go forward.
  EXPECT_EQ(controller.GetEntryCount(), 2);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), 0);
  EXPECT_EQ(controller.GetPendingEntryIndex(), 1);
  EXPECT_TRUE(controller.GetLastCommittedEntry());
  EXPECT_TRUE(controller.GetPendingEntry());
  EXPECT_TRUE(controller.CanGoBack());
  EXPECT_FALSE(controller.CanGoForward());

  forward_navigation->Commit();
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;
  EXPECT_EQ(0U, navigation_list_pruned_counter_);

  EXPECT_EQ(controller.GetEntryCount(), 2);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), 1);
  EXPECT_EQ(controller.GetPendingEntryIndex(), -1);
  EXPECT_TRUE(controller.GetLastCommittedEntry());
  EXPECT_FALSE(controller.GetPendingEntry());
  EXPECT_TRUE(controller.CanGoBack());
  EXPECT_FALSE(controller.CanGoForward());
}

// A redirect right off the bat should be a NEW_ENTRY.
TEST_F(NavigationControllerTest, ImmediateRedirect) {
  NavigationControllerImpl& controller = controller_impl();

  const GURL url1("http://foo1");
  const GURL url2("http://foo2");  // Redirection target

  // First request
  auto navigation =
      NavigationSimulatorImpl::CreateBrowserInitiated(url1, contents());
  navigation->Start();
  EXPECT_TRUE(controller.GetPendingEntry());
  EXPECT_EQ(controller.GetPendingEntryIndex(), -1);
  EXPECT_EQ(url1, controller.GetVisibleEntry()->GetURL());

  EXPECT_EQ(0U, navigation_entry_changed_counter_);
  EXPECT_EQ(0U, navigation_list_pruned_counter_);

  LoadCommittedDetailsObserver observer(contents());
  navigation->Redirect(GURL("http://foo2"));
  navigation->Commit();
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;

  EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_NEW_ENTRY, observer.navigation_type());
  EXPECT_EQ(controller.GetEntryCount(), 1);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), 0);
  EXPECT_TRUE(controller.GetLastCommittedEntry());
  EXPECT_EQ(controller.GetPendingEntryIndex(), -1);
  EXPECT_FALSE(controller.GetPendingEntry());
  EXPECT_EQ(url2, controller.GetVisibleEntry()->GetURL());

  EXPECT_FALSE(controller.CanGoBack());
  EXPECT_FALSE(controller.CanGoForward());
}

// If something is pumping the event loop in the browser process and is loading
// pages rapidly one after the other, there can be a race with two closely-
// spaced load requests. Once the first load request is sent, will the renderer
// be fast enough to get the load committed, send a DidCommitProvisionalLoad
// IPC, and have the browser process handle that IPC before the caller makes
// another load request, replacing the pending entry of the first request?
//
// This test is about what happens in such a race when that pending entry
// replacement happens. If it happens, and the first load had the same URL as
// the page before it, we must make sure that the replacement of the pending
// entry correctly results in an EXISTING_ENTRY classification.
//
// (This is a unit test rather than a browser test because it's not currently
// possible to force this sequence of events with a browser test.)
TEST_F(NavigationControllerTest,
       NavigationTypeClassification_ExistingEntryRace) {
  NavigationControllerImpl& controller = controller_impl();
  const GURL url1("http://foo1");
  const GURL url2("http://foo2");

  // Start with a loaded page.
  NavigationSimulator::NavigateAndCommitFromDocument(url1, main_test_rfh());
  EXPECT_EQ(nullptr, controller_impl().GetPendingEntry());

  // Start a load of the same page again.
  auto navigation1 =
      NavigationSimulatorImpl::CreateBrowserInitiated(url1, contents());
  navigation1->ReadyToCommit();
  int entry_id1 = controller.GetPendingEntry()->GetUniqueID();

  // Before it can commit, start loading a different page...
  auto navigation2 =
      NavigationSimulator::CreateBrowserInitiated(url2, contents());
  navigation2->Start();
  int entry_id2 = controller.GetPendingEntry()->GetUniqueID();
  EXPECT_NE(entry_id1, entry_id2);

  // ... and now the renderer sends a commit for the first navigation.
  LoadCommittedDetailsObserver observer(contents());
  navigation1->Commit();
  EXPECT_EQ(NAVIGATION_TYPE_MAIN_FRAME_EXISTING_ENTRY,
            observer.navigation_type());
}

// Tests navigation via link click within a subframe. A new navigation entry
// should be created.
TEST_F(NavigationControllerTest, NewSubframe) {
  NavigationControllerImpl& controller = controller_impl();

  const GURL url1("http://foo1");
  NavigationSimulator::NavigateAndCommitFromDocument(url1, main_test_rfh());
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;

  // Prereq: add a subframe with an initial auto-subframe navigation.
  const GURL subframe_url("http://foo1/subframe");
  TestRenderFrameHost* subframe = main_test_rfh()->AppendChild("subframe");
  NavigationSimulator::NavigateAndCommitFromDocument(subframe_url, subframe);
  EXPECT_EQ(1u, navigation_entry_changed_counter_);

  // Now do a new navigation in the frame.
  const GURL url2("http://foo2");
  LoadCommittedDetailsObserver observer(contents());
  NavigationSimulator::NavigateAndCommitFromDocument(url2, subframe);
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;
  EXPECT_EQ(url1, observer.previous_primary_main_frame_url());
  EXPECT_FALSE(observer.is_same_document());
  EXPECT_FALSE(observer.is_main_frame());

  // The new entry should be appended.
  NavigationEntryImpl* entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(2, controller.GetEntryCount());

  // New entry should refer to the new page, but the old URL (entries only
  // reflect the toplevel URL).
  EXPECT_EQ(url1, entry->GetURL());

  // The entry should have a subframe FrameNavigationEntry.
  ASSERT_EQ(1U, entry->root_node()->children.size());
  EXPECT_EQ(url2, entry->root_node()->children[0]->frame_entry->url());
}

// Auto subframes are ones the page loads automatically like ads. They should
// not create new navigation entries.
// TODO(creis): Test updating entries for history auto subframe navigations.
TEST_F(NavigationControllerTest, AutoSubframe) {
  NavigationControllerImpl& controller = controller_impl();

  const GURL url1("http://foo/1");
  NavigationSimulator::NavigateAndCommitFromDocument(url1, main_test_rfh());
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;
  constexpr auto kOwnerType = blink::FrameOwnerElementType::kIframe;
  // Add a subframe and navigate it.
  std::string unique_name0("uniqueName0");
  main_test_rfh()->OnCreateChildFrame(
      process()->GetNextRoutingID(),
      TestRenderFrameHost::CreateStubFrameRemote(),
      TestRenderFrameHost::CreateStubBrowserInterfaceBrokerReceiver(),
      TestRenderFrameHost::CreateStubPolicyContainerBindParams(),
      TestRenderFrameHost::CreateStubAssociatedInterfaceProviderReceiver(),
      blink::mojom::TreeScopeType::kDocument, std::string(), unique_name0,
      false, blink::LocalFrameToken(), base::UnguessableToken::Create(),
      blink::DocumentToken(), blink::FramePolicy(),
      blink::mojom::FrameOwnerProperties(), kOwnerType, ukm::kInvalidSourceId);
  TestRenderFrameHost* subframe =
      static_cast<TestRenderFrameHost*>(contents()
                                            ->GetPrimaryFrameTree()
                                            .root()
                                            ->child_at(0)
                                            ->current_frame_host());
  const GURL url2("http://foo/2");
  {
    // Navigating should do nothing.
    auto subframe_navigation =
        NavigationSimulator::CreateRendererInitiated(url2, subframe);
    subframe_navigation->SetTransition(ui::PAGE_TRANSITION_AUTO_SUBFRAME);
    subframe_navigation->Commit();

    // We notify of a PageState update here rather than during UpdateState for
    // auto subframe navigations.
    EXPECT_EQ(1u, navigation_entry_changed_counter_);
    navigation_entry_changed_counter_ = 0;
  }

  // There should still be only one entry.
  EXPECT_EQ(1, controller.GetEntryCount());
  NavigationEntryImpl* entry = controller.GetLastCommittedEntry();
  EXPECT_EQ(url1, entry->GetURL());
  FrameNavigationEntry* root_entry = entry->root_node()->frame_entry.get();
  EXPECT_EQ(url1, root_entry->url());

  // The entry should now have a subframe FrameNavigationEntry.
  ASSERT_EQ(1U, entry->root_node()->children.size());
  FrameNavigationEntry* frame_entry =
      entry->root_node()->children[0]->frame_entry.get();
  EXPECT_EQ(url2, frame_entry->url());

  // Add a second subframe and navigate.
  std::string unique_name1("uniqueName1");
  main_test_rfh()->OnCreateChildFrame(
      process()->GetNextRoutingID(),
      TestRenderFrameHost::CreateStubFrameRemote(),
      TestRenderFrameHost::CreateStubBrowserInterfaceBrokerReceiver(),
      TestRenderFrameHost::CreateStubPolicyContainerBindParams(),
      TestRenderFrameHost::CreateStubAssociatedInterfaceProviderReceiver(),
      blink::mojom::TreeScopeType::kDocument, std::string(), unique_name1,
      false, blink::LocalFrameToken(), base::UnguessableToken::Create(),
      blink::DocumentToken(), blink::FramePolicy(),
      blink::mojom::FrameOwnerProperties(), kOwnerType, ukm::kInvalidSourceId);
  TestRenderFrameHost* subframe2 =
      static_cast<TestRenderFrameHost*>(contents()
                                            ->GetPrimaryFrameTree()
                                            .root()
                                            ->child_at(1)
                                            ->current_frame_host());
  const GURL url3("http://foo/3");
  {
    // Navigating should do nothing.
    auto subframe_navigation =
        NavigationSimulator::CreateRendererInitiated(url3, subframe2);
    subframe_navigation->SetTransition(ui::PAGE_TRANSITION_AUTO_SUBFRAME);
    subframe_navigation->Commit();

    // We notify of a PageState update here rather than during UpdateState for
    // auto subframe navigations.
    EXPECT_EQ(1u, navigation_entry_changed_counter_);
    navigation_entry_changed_counter_ = 0;
  }

  // There should still be only one entry, mostly unchanged.
  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(entry, controller.GetLastCommittedEntry());
  EXPECT_EQ(url1, entry->GetURL());
  EXPECT_EQ(root_entry, entry->root_node()->frame_entry.get());
  EXPECT_EQ(url1, root_entry->url());

  // The entry should now have 2 subframe FrameNavigationEntries.
  ASSERT_EQ(2U, entry->root_node()->children.size());
  FrameNavigationEntry* new_frame_entry =
      entry->root_node()->children[1]->frame_entry.get();
  EXPECT_EQ(url3, new_frame_entry->url());

  // Add a nested subframe and navigate.
  std::string unique_name2("uniqueName2");
  subframe->OnCreateChildFrame(
      process()->GetNextRoutingID(),
      TestRenderFrameHost::CreateStubFrameRemote(),
      TestRenderFrameHost::CreateStubBrowserInterfaceBrokerReceiver(),
      TestRenderFrameHost::CreateStubPolicyContainerBindParams(),
      TestRenderFrameHost::CreateStubAssociatedInterfaceProviderReceiver(),
      blink::mojom::TreeScopeType::kDocument, std::string(), unique_name2,
      false, blink::LocalFrameToken(), base::UnguessableToken::Create(),
      blink::DocumentToken(), blink::FramePolicy(),
      blink::mojom::FrameOwnerProperties(), kOwnerType, ukm::kInvalidSourceId);
  TestRenderFrameHost* subframe3 =
      static_cast<TestRenderFrameHost*>(contents()
                                            ->GetPrimaryFrameTree()
                                            .root()
                                            ->child_at(0)
                                            ->child_at(0)
                                            ->current_frame_host());
  const GURL url4("http://foo/4");
  {
    // Navigating should do nothing.
    auto subframe_navigation =
        NavigationSimulator::CreateRendererInitiated(url4, subframe3);
    subframe_navigation->SetTransition(ui::PAGE_TRANSITION_AUTO_SUBFRAME);
    subframe_navigation->Commit();

    // We notify of a PageState update here rather than during UpdateState for
    // auto subframe navigations.
    EXPECT_EQ(1u, navigation_entry_changed_counter_);
    navigation_entry_changed_counter_ = 0;
  }

  // There should still be only one entry, mostly unchanged.
  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(entry, controller.GetLastCommittedEntry());
  EXPECT_EQ(url1, entry->GetURL());
  EXPECT_EQ(root_entry, entry->root_node()->frame_entry.get());
  EXPECT_EQ(url1, root_entry->url());

  // The entry should now have a nested FrameNavigationEntry.
  EXPECT_EQ(2U, entry->root_node()->children.size());
  ASSERT_EQ(1U, entry->root_node()->children[0]->children.size());
  new_frame_entry =
      entry->root_node()->children[0]->children[0]->frame_entry.get();
  EXPECT_EQ(url4, new_frame_entry->url());
}

// Tests navigation and then going back to a subframe navigation.
TEST_F(NavigationControllerTest, BackSubframe) {
  NavigationControllerImpl& controller = controller_impl();

  // Main page.
  const GURL url1("http://foo1");
  NavigationSimulator::NavigateAndCommitFromDocument(url1, main_test_rfh());
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  NavigationEntry* entry1 = controller.GetLastCommittedEntry();
  navigation_entry_committed_counter_ = 0;

  // Prereq: add a subframe with an initial auto-subframe navigation.
  std::string unique_name("uniqueName0");
  main_test_rfh()->OnCreateChildFrame(
      process()->GetNextRoutingID(),
      TestRenderFrameHost::CreateStubFrameRemote(),
      TestRenderFrameHost::CreateStubBrowserInterfaceBrokerReceiver(),
      TestRenderFrameHost::CreateStubPolicyContainerBindParams(),
      TestRenderFrameHost::CreateStubAssociatedInterfaceProviderReceiver(),
      blink::mojom::TreeScopeType::kDocument, std::string(), unique_name, false,
      blink::LocalFrameToken(), base::UnguessableToken::Create(),
      blink::DocumentToken(), blink::FramePolicy(),
      blink::mojom::FrameOwnerProperties(),
      blink::FrameOwnerElementType::kIframe, ukm::kInvalidSourceId);
  FrameTreeNode* subframe =
      contents()->GetPrimaryFrameTree().root()->child_at(0);
  TestRenderFrameHost* subframe_rfh =
      static_cast<TestRenderFrameHost*>(subframe->current_frame_host());
  const GURL subframe_url("http://foo1/subframe");

  // Navigating should do nothing.
  auto navigation1 =
      NavigationSimulator::CreateRendererInitiated(subframe_url, subframe_rfh);
  navigation1->SetTransition(ui::PAGE_TRANSITION_AUTO_SUBFRAME);
  navigation1->Commit();

  // We notify of a PageState update here rather than during UpdateState for
  // auto subframe navigations.
  EXPECT_EQ(1u, navigation_entry_changed_counter_);

  // First manual subframe_rfh navigation.
  const GURL url2("http://foo2");
  auto navigation2 =
      NavigationSimulator::CreateRendererInitiated(url2, subframe_rfh);
  navigation2->SetTransition(ui::PAGE_TRANSITION_MANUAL_SUBFRAME);
  navigation2->Commit();
  NavigationEntryImpl* entry2 = controller.GetLastCommittedEntry();
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;
  EXPECT_EQ(2, controller.GetEntryCount());

  // The entry should have a subframe FrameNavigationEntry.
  ASSERT_EQ(1U, entry2->root_node()->children.size());
  EXPECT_EQ(url2, entry2->root_node()->children[0]->frame_entry->url());

  // Second manual subframe navigation should also make a new entry.
  const GURL url3("http://foo3");
  subframe_rfh =
      static_cast<TestRenderFrameHost*>(subframe->current_frame_host());
  auto navigation3 =
      NavigationSimulator::CreateRendererInitiated(url3, subframe_rfh);
  navigation3->SetTransition(ui::PAGE_TRANSITION_MANUAL_SUBFRAME);
  navigation3->Commit();
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;
  NavigationEntryImpl* entry3 = controller.GetLastCommittedEntry();
  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(2, controller.GetCurrentEntryIndex());

  // The entry should have a subframe FrameNavigationEntry.
  ASSERT_EQ(1U, entry3->root_node()->children.size());
  EXPECT_EQ(url3, entry3->root_node()->children[0]->frame_entry->url());

  // Go back one.
  controller.GoToOffset(-1);
  auto back_navigation1 =
      NavigationSimulatorImpl::CreateFromPendingInFrame(subframe);
  back_navigation1->SetTransition(ui::PAGE_TRANSITION_AUTO_SUBFRAME);
  back_navigation1->Commit();
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;
  EXPECT_EQ(entry2, controller.GetLastCommittedEntry());
  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetCurrentEntryIndex());
  EXPECT_EQ(-1, controller.GetPendingEntryIndex());
  EXPECT_FALSE(controller.GetPendingEntry());

  // Go back one more.
  controller.GoToOffset(-1);
  auto back_navigation2 =
      NavigationSimulatorImpl::CreateFromPendingInFrame(subframe);
  back_navigation2->SetTransition(ui::PAGE_TRANSITION_AUTO_SUBFRAME);
  back_navigation2->Commit();
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;
  EXPECT_EQ(entry1, controller.GetLastCommittedEntry());
  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(0, controller.GetCurrentEntryIndex());
  EXPECT_EQ(-1, controller.GetPendingEntryIndex());
  EXPECT_FALSE(controller.GetPendingEntry());
}

TEST_F(NavigationControllerTest, LinkClick) {
  NavigationControllerImpl& controller = controller_impl();

  const GURL url1("http://foo1");
  const GURL url2("http://foo2");

  NavigationSimulator::NavigateAndCommitFromDocument(url1, main_test_rfh());
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;

  // Simulate a user gesture.
  main_test_rfh()->frame_tree_node()->UpdateUserActivationState(
      blink::mojom::UserActivationUpdateType::kNotifyActivation,
      blink::mojom::UserActivationNotificationType::kTest);

  NavigationSimulator::NavigateAndCommitFromDocument(url2, main_test_rfh());
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;

  // Should have produced a new session history entry.
  EXPECT_EQ(controller.GetEntryCount(), 2);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), 1);
  EXPECT_EQ(controller.GetPendingEntryIndex(), -1);
  EXPECT_TRUE(controller.GetLastCommittedEntry());
  EXPECT_FALSE(controller.GetPendingEntry());
  EXPECT_TRUE(controller.CanGoBack());
  EXPECT_FALSE(controller.CanGoForward());
}

TEST_F(NavigationControllerTest, SameDocument) {
  NavigationControllerImpl& controller = controller_impl();

  // Main page.
  const GURL url1("http://foo");
  NavigationSimulator::NavigateAndCommitFromDocument(url1, main_test_rfh());
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;

  // Ensure renderer-initiated main frame navigation to same url replaces the
  // current entry. This behavior differs from the browser-initiated case.
  LoadCommittedDetailsObserver observer(contents());
  auto renderer_initiated_navigation =
      NavigationSimulatorImpl::CreateRendererInitiated(url1, main_test_rfh());
  renderer_initiated_navigation->set_should_replace_current_entry(true);
  renderer_initiated_navigation->Commit();
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;
  EXPECT_FALSE(observer.is_same_document());
  EXPECT_TRUE(observer.did_replace_entry());
  EXPECT_EQ(1, controller.GetEntryCount());

  // Fragment navigation to a new page.
  const GURL url2("http://foo#a");
  auto same_document_navigation =
      NavigationSimulator::CreateRendererInitiated(url2, main_test_rfh());
  same_document_navigation->CommitSameDocument();

  // This should generate a new entry.
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;
  EXPECT_TRUE(observer.is_same_document());
  EXPECT_FALSE(observer.did_replace_entry());
  EXPECT_EQ(2, controller.GetEntryCount());

  // Go back one.
  NavigationSimulator::GoBack(contents());
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;
  EXPECT_TRUE(observer.is_same_document());
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(0, controller.GetCurrentEntryIndex());
  EXPECT_EQ(url1, controller.GetLastCommittedEntry()->GetURL());

  // Go forward.
  NavigationSimulator::GoForward(contents());
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetCurrentEntryIndex());
  EXPECT_EQ(url2, controller.GetVisibleEntry()->GetURL());

  // Now go back and forward again. This is to work around a bug where we would
  // compare the incoming URL with the last committed entry rather than the
  // one identified by an existing page ID. This would result in the second URL
  // losing the reference fragment when you navigate away from it and then back.
  NavigationSimulator::GoBack(contents());
  NavigationSimulator::GoForward(contents());
  EXPECT_EQ(2U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;
  EXPECT_EQ(url2, controller.GetVisibleEntry()->GetURL());

  // Finally, navigate to an unrelated URL to make sure same_document is not
  // sticky.
  const GURL url3("http://bar");
  NavigationSimulator::NavigateAndCommitFromDocument(url3, main_test_rfh());
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;
  EXPECT_FALSE(observer.is_same_document());
  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(2, controller.GetCurrentEntryIndex());
}

TEST_F(NavigationControllerTest, SameDocument_Replace) {
  NavigationControllerImpl& controller = controller_impl();

  // Main page.
  const GURL url1("http://foo");
  NavigationSimulator::NavigateAndCommitFromDocument(url1, main_test_rfh());
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;

  // First navigation (using location.replace).
  const GURL url2("http://foo#a");
  auto params = mojom::DidCommitProvisionalLoadParams::New();
  params->did_create_new_entry = false;
  params->url = url2;
  params->origin = url::Origin::Create(url2);
  params->referrer = blink::mojom::Referrer::New();
  params->transition = ui::PAGE_TRANSITION_LINK;
  params->should_update_history = true;
  params->method = "GET";
  params->page_state = blink::PageState::CreateFromURL(url2);
  params->post_id = -1;

  // This should NOT generate a new entry, nor prune the list.
  LoadCommittedDetailsObserver observer(contents());
  main_test_rfh()->SendDidCommitSameDocumentNavigation(
      std::move(params), blink::mojom::SameDocumentNavigationType::kFragment,
      /*should_replace_current_entry=*/true);
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;
  EXPECT_TRUE(observer.is_same_document());
  EXPECT_TRUE(observer.did_replace_entry());
  EXPECT_EQ(1, controller.GetEntryCount());
}

TEST_F(NavigationControllerTest, PushStateWithOnlyInitialEntry) {
  ASSERT_TRUE(controller_impl().GetLastCommittedEntry()->IsInitialEntry());
  GURL url("http://foo");
  auto params = mojom::DidCommitProvisionalLoadParams::New();
  params->did_create_new_entry = true;
  params->url = url;
  params->referrer = blink::mojom::Referrer::New();
  params->page_state = blink::PageState::CreateFromURL(url);
  params->method = "GET";
  params->should_update_history = true;
  params->post_id = -1;
  main_test_rfh()->SendRendererInitiatedNavigationRequest(
      url, false /* has_user_gesture */);
  main_test_rfh()->PrepareForCommit();
  contents()->GetPrimaryMainFrame()->SendNavigateWithParams(
      std::move(params), true /* was_within_same_document */);
  // We pass if we don't crash.
}

// Tests that we limit the number of navigation entries created correctly.
TEST_F(NavigationControllerTest, EnforceMaxNavigationCount) {
  NavigationControllerImpl& controller = controller_impl();
  size_t original_count = NavigationControllerImpl::max_entry_count();
  const int kMaxEntryCount = 5;

  NavigationControllerImpl::set_max_entry_count_for_testing(kMaxEntryCount);

  int url_index;
  // Load up to the max count, all entries should be there.
  for (url_index = 0; url_index < kMaxEntryCount; url_index++) {
    GURL url(base::StringPrintf("http://www.a.com/%d", url_index));
    NavigationSimulator::NavigateAndCommitFromBrowser(contents(), url);
  }

  EXPECT_EQ(controller.GetEntryCount(), kMaxEntryCount);

  // Navigate some more.
  GURL url(base::StringPrintf("http://www.a.com/%d", url_index));
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), url);
  url_index++;

  // We should have got a pruned navigation.
  EXPECT_EQ(1U, navigation_list_pruned_counter_);
  EXPECT_EQ(0, last_navigation_entry_pruned_details_.index);
  EXPECT_EQ(1, last_navigation_entry_pruned_details_.count);

  // We expect http://www.a.com/0 to be gone.
  EXPECT_EQ(controller.GetEntryCount(), kMaxEntryCount);
  EXPECT_EQ(controller.GetEntryAtIndex(0)->GetURL(),
            GURL("http://www.a.com/1"));

  // More navigations.
  for (int i = 0; i < 3; i++) {
    url = GURL(base::StringPrintf("http://www.a.com/%d", url_index));
    NavigationSimulator::NavigateAndCommitFromBrowser(contents(), url);
    url_index++;
  }
  EXPECT_EQ(controller.GetEntryCount(), kMaxEntryCount);
  EXPECT_EQ(controller.GetEntryAtIndex(0)->GetURL(),
            GURL("http://www.a.com/4"));

  NavigationControllerImpl::set_max_entry_count_for_testing(original_count);
}

// Tests that we can do a restore and navigate to the restored entries and
// everything is updated properly. This can be tricky since there is no
// SiteInstance for the entries created initially.
TEST_F(NavigationControllerTest, RestoreNavigate) {
  // Create a NavigationController with a restored set of tabs.
  GURL url("http://foo");
  std::vector<std::unique_ptr<NavigationEntry>> entries;
  std::unique_ptr<NavigationEntryImpl> entry =
      NavigationEntryImpl::FromNavigationEntry(
          NavigationController::CreateNavigationEntry(
              url, Referrer(), /* initiator_origin= */ std::nullopt,
              /* initiator_base_url= */ std::nullopt,
              ui::PAGE_TRANSITION_RELOAD, false, std::string(),
              browser_context(), nullptr /* blob_url_loader_factory */));
  entry->SetTitle(u"Title");
  const base::Time timestamp = base::Time::Now();
  entry->SetTimestamp(timestamp);
  NavigationEntryRestoreContextImpl context;
  entry->SetPageState(blink::PageState::CreateFromURL(url), &context);
  entries.push_back(std::move(entry));

  std::unique_ptr<WebContents> our_contents =
      WebContents::Create(WebContents::CreateParams(browser_context()));
  WebContentsImpl* raw_our_contents =
      static_cast<WebContentsImpl*>(our_contents.get());
  NavigationControllerImpl& our_controller = raw_our_contents->GetController();
  our_controller.Restore(0, RestoreType::kRestored, &entries);
  ASSERT_EQ(0u, entries.size());

  // Before navigating to the restored entry, it should have a restore_type
  // and no SiteInstance.
  ASSERT_EQ(1, our_controller.GetEntryCount());
  EXPECT_EQ(RestoreType::kRestored,
            our_controller.GetEntryAtIndex(0)->restore_type());
  EXPECT_FALSE(our_controller.GetEntryAtIndex(0)->site_instance());

  // After navigating, we should have one entry, and it should be "pending".
  EXPECT_TRUE(our_controller.NeedsReload());
  our_controller.LoadIfNecessary();
  EXPECT_EQ(1, our_controller.GetEntryCount());
  EXPECT_EQ(our_controller.GetEntryAtIndex(0),
            our_controller.GetPendingEntry());

  // Timestamp should remain the same before the navigation finishes.
  EXPECT_EQ(timestamp, our_controller.GetEntryAtIndex(0)->GetTimestamp());

  // Say we navigated to that entry.
  auto navigation =
      NavigationSimulator::CreateFromPending(raw_our_contents->GetController());
  navigation->Commit();

  // There should be no longer any pending entry and one committed one. This
  // means that we were able to locate the entry, assign its site instance, and
  // commit it properly.
  EXPECT_EQ(1, our_controller.GetEntryCount());
  EXPECT_EQ(0, our_controller.GetLastCommittedEntryIndex());
  EXPECT_FALSE(our_controller.GetPendingEntry());
  if (AreDefaultSiteInstancesEnabled()) {
    // Verify we get the default SiteInstance since |url| does not require a
    // dedicated process.
    EXPECT_TRUE(our_controller.GetLastCommittedEntry()
                    ->site_instance()
                    ->IsDefaultSiteInstance());
  } else {
    EXPECT_EQ(
        url,
        our_controller.GetLastCommittedEntry()->site_instance()->GetSiteURL());
  }
  EXPECT_EQ(RestoreType::kNotRestored,
            our_controller.GetEntryAtIndex(0)->restore_type());

  // Timestamp should have been updated.
  EXPECT_GE(our_controller.GetEntryAtIndex(0)->GetTimestamp(), timestamp);
}

// Tests that we can still navigate to a restored entry after a different
// navigation fails and clears the pending entry.  http://crbug.com/90085
TEST_F(NavigationControllerTest, RestoreNavigateAfterFailure) {
  // Create a NavigationController with a restored set of tabs.
  GURL url("http://foo");
  std::vector<std::unique_ptr<NavigationEntry>> entries;
  std::unique_ptr<NavigationEntryImpl> new_entry =
      NavigationEntryImpl::FromNavigationEntry(
          NavigationController::CreateNavigationEntry(
              url, Referrer(), /* initiator_origin= */ std::nullopt,
              /* initiator_base_url= */ std::nullopt,
              ui::PAGE_TRANSITION_RELOAD, false, std::string(),
              browser_context(), nullptr /* blob_url_loader_factory */));
  new_entry->SetTitle(u"Title");
  NavigationEntryRestoreContextImpl context;
  new_entry->SetPageState(blink::PageState::CreateFromURL(url), &context);
  entries.push_back(std::move(new_entry));

  std::unique_ptr<WebContents> our_contents =
      WebContents::Create(WebContents::CreateParams(browser_context()));
  WebContentsImpl* raw_our_contents =
      static_cast<WebContentsImpl*>(our_contents.get());
  NavigationControllerImpl& our_controller = raw_our_contents->GetController();
  our_controller.Restore(0, RestoreType::kRestored, &entries);
  ASSERT_EQ(0u, entries.size());

  // Ensure the RenderFrame is initialized before simulating events coming from
  // it.
  main_test_rfh()->InitializeRenderFrameIfNeeded();

  // Before navigating to the restored entry, it should have a restore_type
  // and no SiteInstance.
  EXPECT_EQ(RestoreType::kRestored,
            our_controller.GetEntryAtIndex(0)->restore_type());
  EXPECT_FALSE(our_controller.GetEntryAtIndex(0)->site_instance());

  // After navigating, we should have one entry, and it should be "pending".
  EXPECT_TRUE(our_controller.NeedsReload());
  our_controller.LoadIfNecessary();
  auto restore_navigation =
      NavigationSimulator::CreateFromPending(raw_our_contents->GetController());
  restore_navigation->ReadyToCommit();
  EXPECT_EQ(1, our_controller.GetEntryCount());
  EXPECT_EQ(our_controller.GetEntryAtIndex(0),
            our_controller.GetPendingEntry());

  // This pending navigation may have caused a different navigation to fail,
  // which causes the pending entry to be cleared.
  NavigationSimulator::NavigateAndFailFromDocument(url, net::ERR_ABORTED,
                                                   main_test_rfh());
  // Now the pending restored entry commits.
  restore_navigation->Commit();

  // There should be no pending entry and one committed one.
  EXPECT_EQ(1, our_controller.GetEntryCount());
  EXPECT_EQ(0, our_controller.GetLastCommittedEntryIndex());
  EXPECT_FALSE(our_controller.GetPendingEntry());
  if (AreDefaultSiteInstancesEnabled()) {
    // Verify we get the default SiteInstance since |url| does not require a
    // dedicated process.
    EXPECT_TRUE(our_controller.GetLastCommittedEntry()
                    ->site_instance()
                    ->IsDefaultSiteInstance());
  } else {
    EXPECT_EQ(
        url,
        our_controller.GetLastCommittedEntry()->site_instance()->GetSiteURL());
  }
  EXPECT_EQ(RestoreType::kNotRestored,
            our_controller.GetEntryAtIndex(0)->restore_type());
}

TEST_F(NavigationControllerTest, RemoveEntry) {
  NavigationControllerImpl& controller = controller_impl();
  const GURL url1("http://foo/1");
  const GURL url2("http://foo/2");
  const GURL url3("http://foo/3");
  const GURL url4("http://foo/4");
  const GURL url5("http://foo/5");
  const GURL pending_url("http://foo/pending");
  const GURL default_url("http://foo/default");

  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), url1);
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), url2);
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), url3);
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), url4);
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), url5);

  // Try to remove the last entry.  Will fail because it is the current entry.
  EXPECT_FALSE(controller.RemoveEntryAtIndex(controller.GetEntryCount() - 1));
  EXPECT_EQ(5, controller.GetEntryCount());
  EXPECT_EQ(4, controller.GetLastCommittedEntryIndex());

  // Go back, but don't commit yet. Check that we can't delete the current
  // and pending entries.
  auto back_navigation = NavigationSimulator::CreateHistoryNavigation(
      -1, contents(), false /* is_renderer_initiated */);
  back_navigation->Start();
  EXPECT_FALSE(controller.RemoveEntryAtIndex(controller.GetEntryCount() - 1));
  EXPECT_FALSE(controller.RemoveEntryAtIndex(controller.GetEntryCount() - 2));

  // Now commit and delete the last entry.
  back_navigation->Commit();
  EXPECT_TRUE(controller.RemoveEntryAtIndex(controller.GetEntryCount() - 1));
  EXPECT_EQ(4, controller.GetEntryCount());
  EXPECT_EQ(3, controller.GetLastCommittedEntryIndex());
  EXPECT_FALSE(controller.GetPendingEntry());

  // Remove an entry which is not the last committed one.
  EXPECT_TRUE(controller.RemoveEntryAtIndex(0));
  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(2, controller.GetLastCommittedEntryIndex());
  EXPECT_FALSE(controller.GetPendingEntry());

  // Remove the 2 remaining entries.
  controller.RemoveEntryAtIndex(1);
  controller.RemoveEntryAtIndex(0);

  // This should leave us with only the last committed entry.
  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());
}

TEST_F(NavigationControllerTest, RemoveEntryWithPending) {
  NavigationControllerImpl& controller = controller_impl();
  const GURL url1("http://foo/1");
  const GURL url2("http://foo/2");
  const GURL url3("http://foo/3");
  const GURL default_url("http://foo/default");

  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), url1);
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), url2);
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), url3);

  // Go back, but don't commit yet. Check that we can't delete the current
  // and pending entries.
  auto back_navigation = NavigationSimulator::CreateHistoryNavigation(
      -1, contents(), false /* is_renderer_initiated */);
  back_navigation->Start();
  EXPECT_FALSE(controller.RemoveEntryAtIndex(2));
  EXPECT_FALSE(controller.RemoveEntryAtIndex(1));

  // Remove the first entry, while there is a pending entry.  This is expected
  // to discard the pending entry.
  EXPECT_TRUE(controller.RemoveEntryAtIndex(0));
  EXPECT_FALSE(controller.GetPendingEntry());
  EXPECT_EQ(-1, controller.GetPendingEntryIndex());

  // We should update the last committed entry index.
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());

  // Now commit and ensure we land on the right entry.
  back_navigation->Commit();
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());
  EXPECT_FALSE(controller.GetPendingEntry());
}

// Ensure that renderer initiated pending entries get replaced, so that we
// don't show a stale virtual URL when a navigation commits.
// See http://crbug.com/266922.
TEST_F(NavigationControllerTest, RendererInitiatedPendingEntries) {
  NavigationControllerImpl& controller = controller_impl();

  const GURL url1("nonexistent:12121");
  const GURL url1_fixed("http://nonexistent:12121/");
  const GURL url2("http://foo");

  // We create pending entries for renderer-initiated navigations so that we
  // can show them in new tabs when it is safe.
  auto navigation1 =
      NavigationSimulator::CreateRendererInitiated(url1, main_test_rfh());
  navigation1->ReadyToCommit();

  // Simulate what happens if a BrowserURLHandler rewrites the URL, causing
  // the virtual URL to differ from the URL.
  controller.GetPendingEntry()->SetURL(url1_fixed);
  controller.GetPendingEntry()->SetVirtualURL(url1);

  EXPECT_EQ(url1_fixed, controller.GetPendingEntry()->GetURL());
  EXPECT_EQ(url1, controller.GetPendingEntry()->GetVirtualURL());
  EXPECT_TRUE(controller.GetPendingEntry()->is_renderer_initiated());

  // If the user clicks another link, we should replace the pending entry.
  auto navigation2 =
      NavigationSimulator::CreateRendererInitiated(url2, main_test_rfh());
  navigation2->ReadyToCommit();
  EXPECT_EQ(url2, controller.GetPendingEntry()->GetURL());
  EXPECT_EQ(url2, controller.GetPendingEntry()->GetVirtualURL());

  // Once it commits, the URL and virtual URL should reflect the actual page.
  navigation2->Commit();
  EXPECT_EQ(url2, controller.GetLastCommittedEntry()->GetURL());
  EXPECT_EQ(url2, controller.GetLastCommittedEntry()->GetVirtualURL());

  // We should remember if the pending entry will replace the current one.
  // http://crbug.com/308444.
  auto navigation3 =
      NavigationSimulatorImpl::CreateRendererInitiated(url2, main_test_rfh());
  navigation3->set_should_replace_current_entry(true);
  navigation3->Commit();
  EXPECT_EQ(url2, controller.GetLastCommittedEntry()->GetURL());
}

// Tests that the URLs for renderer-initiated navigations are not displayed to
// the user until the navigation commits, to prevent URL spoof attacks.
// See http://crbug.com/99016.
TEST_F(NavigationControllerTest, DontShowRendererURLUntilCommit) {
  NavigationControllerImpl& controller = controller_impl();

  const GURL url0("http://foo/0");
  const GURL url1("http://foo/1");

  // For typed navigations (browser-initiated), both pending and visible entries
  // should update before commit.
  {
    auto navigation =
        NavigationSimulator::CreateBrowserInitiated(url0, contents());
    navigation->Start();
    EXPECT_EQ(url0, controller.GetPendingEntry()->GetURL());
    EXPECT_EQ(url0, controller.GetVisibleEntry()->GetURL());
    navigation->Commit();
  }

  // For renderer-initiated navigations, the pending entry should update before
  // commit but the visible should not.
  {
    auto navigation =
        NavigationSimulator::CreateRendererInitiated(url1, main_test_rfh());
    navigation->Start();
    EXPECT_EQ(url0, controller.GetVisibleEntry()->GetURL());
    EXPECT_EQ(url1, controller.GetPendingEntry()->GetURL());
    EXPECT_TRUE(controller.GetPendingEntry()->is_renderer_initiated());

    // After commit, both visible should be updated, there should be no pending
    // entry, and we should no longer treat the entry as renderer-initiated.
    navigation->Commit();
    EXPECT_EQ(url1, controller.GetVisibleEntry()->GetURL());
    EXPECT_FALSE(controller.GetPendingEntry());
    EXPECT_FALSE(controller.GetLastCommittedEntry()->is_renderer_initiated());
  }
}

// Tests that the URLs for renderer-initiated navigations in new tabs are
// displayed to the user before commit, as long as the initial about:blank
// page has not been modified.  If so, we must revert to showing about:blank.
// See http://crbug.com/9682.
TEST_F(NavigationControllerTest, ShowRendererURLInNewTabUntilModified) {
  NavigationControllerImpl& controller = controller_impl();

  const GURL url("http://foo");

  // For renderer-initiated navigations in new tabs (with no committed entries),
  // we show the pending entry's URL as long as the about:blank page is not
  // modified.
  auto navigation =
      NavigationSimulatorImpl::CreateBrowserInitiated(url, contents());
  NavigationController::LoadURLParams load_url_params(url);
  load_url_params.initiator_origin = url::Origin();
  load_url_params.transition_type = ui::PAGE_TRANSITION_LINK;
  load_url_params.is_renderer_initiated = true;
  navigation->SetLoadURLParams(&load_url_params);
  navigation->Start();

  EXPECT_EQ(url, controller.GetVisibleEntry()->GetURL());
  EXPECT_EQ(url, controller.GetPendingEntry()->GetURL());
  EXPECT_TRUE(controller.GetPendingEntry()->is_renderer_initiated());
  EXPECT_TRUE(controller.IsInitialNavigation());
  EXPECT_FALSE(contents()->HasAccessedInitialDocument());

  // There should be no title yet.
  EXPECT_TRUE(contents()->GetTitle().empty());

  // If something else modifies the contents of the about:blank page, then
  // we must revert to showing about:blank to avoid a URL spoof.
  main_test_rfh()->DidAccessInitialMainDocument();
  EXPECT_TRUE(contents()->HasAccessedInitialDocument());
  EXPECT_TRUE(controller.GetVisibleEntry()->IsInitialEntry());
  EXPECT_EQ(url, controller.GetPendingEntry()->GetURL());
}

// Tests that the URLs for browser-initiated navigations in new tabs are
// displayed to the user even after they fail, as long as the initial
// about:blank page has not been modified.  If so, we must revert to showing
// about:blank. See http://crbug.com/355537.
TEST_F(NavigationControllerTest, ShowBrowserURLAfterFailUntilModified) {
  NavigationControllerImpl& controller = controller_impl();

  const GURL url("http://foo");

  // For browser-initiated navigations in new tabs (with no committed entries),
  // we show the pending entry's URL as long as the about:blank page is not
  // modified.  This is possible in cases that the user types a URL into a popup
  // tab created with a slow URL.
  auto navigation =
      NavigationSimulatorImpl::CreateBrowserInitiated(url, contents());
  NavigationController::LoadURLParams load_url_params(url);
  load_url_params.transition_type = ui::PAGE_TRANSITION_TYPED;
  load_url_params.is_renderer_initiated = false;
  navigation->SetLoadURLParams(&load_url_params);
  navigation->Start();

  EXPECT_EQ(url, controller.GetVisibleEntry()->GetURL());
  EXPECT_EQ(url, controller.GetPendingEntry()->GetURL());
  EXPECT_FALSE(controller.GetPendingEntry()->is_renderer_initiated());
  EXPECT_TRUE(controller.IsInitialNavigation());
  EXPECT_FALSE(contents()->HasAccessedInitialDocument());

  // There should be no title yet.
  EXPECT_TRUE(contents()->GetTitle().empty());

  // Suppose it aborts before committing, if it's a 204 or download or due to a
  // stop or a new navigation from the user.  The URL should remain visible.
  main_test_rfh()->frame_tree_node()->navigator().CancelNavigation(
      main_test_rfh()->frame_tree_node(),
      NavigationDiscardReason::kExplicitCancellation);
  EXPECT_EQ(url, controller.GetVisibleEntry()->GetURL());

  // If something else later modifies the contents of the about:blank page, then
  // we must revert to showing about:blank to avoid a URL spoof.
  main_test_rfh()->DidAccessInitialMainDocument();
  EXPECT_TRUE(contents()->HasAccessedInitialDocument());
  EXPECT_TRUE(controller.GetVisibleEntry()->IsInitialEntry());
  EXPECT_FALSE(controller.GetPendingEntry());
}

// Tests that the URLs for renderer-initiated navigations in new tabs are
// displayed to the user even after they fail, as long as the initial
// about:blank page has not been modified.  If so, we must revert to showing
// about:blank. See http://crbug.com/355537.
TEST_F(NavigationControllerTest, ShowRendererURLAfterFailUntilModified) {
  NavigationControllerImpl& controller = controller_impl();

  const GURL url("http://foo");

  // For renderer-initiated navigations in new tabs (with no committed entries),
  // we show the pending entry's URL as long as the about:blank page is not
  // modified.
  auto navigation =
      NavigationSimulator::CreateRendererInitiated(url, main_test_rfh());
  navigation->Start();

  EXPECT_EQ(url, controller.GetVisibleEntry()->GetURL());
  EXPECT_EQ(url, controller.GetPendingEntry()->GetURL());
  EXPECT_TRUE(controller.GetPendingEntry()->is_renderer_initiated());
  EXPECT_TRUE(controller.IsInitialNavigation());
  EXPECT_FALSE(contents()->HasAccessedInitialDocument());

  // There should be no title yet.
  EXPECT_TRUE(contents()->GetTitle().empty());

  // Suppose it aborts before committing, if it's a 204 or download or due to a
  // stop or a new navigation from the user.  The URL should remain visible.
  navigation->Fail(net::ERR_FAILED);
  EXPECT_EQ(url, controller.GetVisibleEntry()->GetURL());

  // If something else later modifies the contents of the about:blank page, then
  // we must revert to showing about:blank to avoid a URL spoof.
  main_test_rfh()->DidAccessInitialMainDocument();
  EXPECT_TRUE(contents()->HasAccessedInitialDocument());
  EXPECT_TRUE(controller.GetVisibleEntry()->IsInitialEntry());
  EXPECT_EQ(url, controller.GetPendingEntry()->GetURL());
}

// Tests that the URLs for renderer-initiated navigations in new tabs are
// displayed to the user after they got canceled, as long as the initial
// about:blank page has not been modified. If so, we must revert to showing
// about:blank. See http://crbug.com/355537.
TEST_F(NavigationControllerTest, ShowRendererURLAfterCancelUntilModified) {
  NavigationControllerImpl& controller = controller_impl();

  const GURL url("http://foo");

  // For renderer-initiated navigations in new tabs (with no committed entries),
  // we show the pending entry's URL as long as the about:blank page is not
  // modified.
  auto navigation =
      NavigationSimulator::CreateRendererInitiated(url, main_test_rfh());
  navigation->Start();

  EXPECT_EQ(url, controller.GetVisibleEntry()->GetURL());
  EXPECT_EQ(url, controller.GetPendingEntry()->GetURL());
  EXPECT_TRUE(controller.GetPendingEntry()->is_renderer_initiated());
  EXPECT_TRUE(controller.IsInitialNavigation());
  EXPECT_FALSE(contents()->HasAccessedInitialDocument());

  // There should be no title yet.
  EXPECT_TRUE(contents()->GetTitle().empty());

  // Suppose it aborts before committing, e.g. due to a new renderer-initiated
  // navigation. The URL should remain visible.
  navigation->AbortFromRenderer();
  EXPECT_EQ(url, controller.GetVisibleEntry()->GetURL());

  // If something else later modifies the contents of the about:blank page, then
  // we must revert to showing about:blank to avoid a URL spoof.
  // Pending entry should also be discarded, because renderer doesn't want to
  // show this page anymore.
  main_test_rfh()->DidAccessInitialMainDocument();
  EXPECT_TRUE(contents()->HasAccessedInitialDocument());
  EXPECT_TRUE(controller.GetVisibleEntry()->IsInitialEntry());
  EXPECT_FALSE(controller.GetPendingEntry());
}

TEST_F(NavigationControllerTest, DontShowRendererURLInNewTabAfterCommit) {
  NavigationControllerImpl& controller = controller_impl();

  const GURL url1("http://foo/eh");
  const GURL url2("http://foo/bee");

  // For renderer-initiated navigations in new tabs (with no committed entries),
  // we show the pending entry's URL as long as the about:blank page is not
  // modified.
  {
    auto navigation =
        NavigationSimulatorImpl::CreateBrowserInitiated(url1, contents());
    NavigationController::LoadURLParams load_url_params(url1);
    load_url_params.initiator_origin = url::Origin();
    load_url_params.transition_type = ui::PAGE_TRANSITION_LINK;
    load_url_params.is_renderer_initiated = true;
    navigation->SetLoadURLParams(&load_url_params);
    navigation->Start();

    EXPECT_EQ(url1, controller.GetVisibleEntry()->GetURL());
    EXPECT_TRUE(controller.GetPendingEntry()->is_renderer_initiated());
    EXPECT_TRUE(controller.IsInitialNavigation());
    EXPECT_FALSE(contents()->HasAccessedInitialDocument());

    navigation->Commit();
  }

  // Now start a new pending navigation.
  {
    auto navigation =
        NavigationSimulatorImpl::CreateBrowserInitiated(url2, contents());
    NavigationController::LoadURLParams load_url_params(url2);
    load_url_params.initiator_origin = url::Origin::Create(url1);
    load_url_params.transition_type = ui::PAGE_TRANSITION_LINK;
    load_url_params.is_renderer_initiated = true;
    navigation->SetLoadURLParams(&load_url_params);
    navigation->Start();

    // We should not consider this an initial navigation, and thus should
    // not show the pending URL.
    EXPECT_FALSE(contents()->HasAccessedInitialDocument());
    EXPECT_FALSE(controller.IsInitialNavigation());
    EXPECT_TRUE(controller.GetVisibleEntry());
    EXPECT_EQ(url1, controller.GetVisibleEntry()->GetURL());
  }
}

// This test verifies that a subframe navigation that would qualify as
// same-document within the main frame, given its URL, has no impact on the
// main frame.
// Original bug: http://crbug.com/5585
TEST_F(NavigationControllerTest, SameSubframe) {
  NavigationControllerImpl& controller = controller_impl();
  // Navigate the main frame.
  const GURL url("http://www.google.com/");
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), url);

  // We should be at the first navigation entry.
  EXPECT_EQ(controller.GetEntryCount(), 1);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), 0);

  // Add and navigate a subframe that is "same-document" with the main frame.
  std::string unique_name("uniqueName0");
  main_test_rfh()->OnCreateChildFrame(
      process()->GetNextRoutingID(),
      TestRenderFrameHost::CreateStubFrameRemote(),
      TestRenderFrameHost::CreateStubBrowserInterfaceBrokerReceiver(),
      TestRenderFrameHost::CreateStubPolicyContainerBindParams(),
      TestRenderFrameHost::CreateStubAssociatedInterfaceProviderReceiver(),
      blink::mojom::TreeScopeType::kDocument, std::string(), unique_name, false,
      blink::LocalFrameToken(), base::UnguessableToken::Create(),
      blink::DocumentToken(), blink::FramePolicy(),
      blink::mojom::FrameOwnerProperties(),
      blink::FrameOwnerElementType::kIframe, ukm::kInvalidSourceId);
  TestRenderFrameHost* subframe =
      static_cast<TestRenderFrameHost*>(contents()
                                            ->GetPrimaryFrameTree()
                                            .root()
                                            ->child_at(0)
                                            ->current_frame_host());
  const GURL subframe_url("http://www.google.com/#");
  NavigationSimulator::NavigateAndCommitFromDocument(subframe_url, subframe);

  // Nothing should have changed.
  EXPECT_EQ(controller.GetEntryCount(), 1);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), 0);
}

// Make sure that on cloning a WebContentsImpl and going back needs_reload is
// false.
TEST_F(NavigationControllerTest, CloneAndGoBack) {
  NavigationControllerImpl& controller = controller_impl();
  const GURL url1("http://foo1");
  const GURL url2("http://foo2");
  const std::u16string title(u"Title");

  NavigateAndCommit(url1);
  controller.GetVisibleEntry()->SetTitle(title);
  NavigateAndCommit(url2);

  std::unique_ptr<WebContents> clone(
      RenderViewHostTestHarness::web_contents()->Clone());

  ASSERT_EQ(2, clone->GetController().GetEntryCount());
  EXPECT_TRUE(clone->GetController().NeedsReload());
  clone->GetController().GoBack();
  // Navigating back should have triggered needs_reload_ to go false.
  EXPECT_FALSE(clone->GetController().NeedsReload());

  // Ensure that the pending URL and its title are visible.
  EXPECT_EQ(url1, clone->GetController().GetVisibleEntry()->GetURL());
  EXPECT_EQ(title, clone->GetTitle());
}

// Make sure that reloading a cloned tab doesn't change its pending entry index.
// See http://crbug.com/234491.
TEST_F(NavigationControllerTest, CloneAndReload) {
  NavigationControllerImpl& controller = controller_impl();
  const GURL url1("http://foo1");
  const GURL url2("http://foo2");
  const std::u16string title(u"Title");

  NavigateAndCommit(url1);
  controller.GetVisibleEntry()->SetTitle(title);
  NavigateAndCommit(url2);

  std::unique_ptr<WebContents> clone(
      RenderViewHostTestHarness::web_contents()->Clone());
  clone->GetController().LoadIfNecessary();

  ASSERT_EQ(2, clone->GetController().GetEntryCount());
  EXPECT_EQ(1, clone->GetController().GetPendingEntryIndex());

  clone->GetController().Reload(ReloadType::NORMAL, true);
  EXPECT_EQ(1, clone->GetController().GetPendingEntryIndex());
}

// Test requesting and triggering a lazy reload.
TEST_F(NavigationControllerTest, LazyReload) {
  NavigationControllerImpl& controller = controller_impl();
  const GURL url("http://foo");
  NavigateAndCommit(url);
  ASSERT_FALSE(controller.NeedsReload());
  EXPECT_FALSE(ui::PageTransitionTypeIncludingQualifiersIs(
      controller.GetLastCommittedEntry()->GetTransitionType(),
      ui::PAGE_TRANSITION_RELOAD));

  // Request a reload to happen when the controller becomes active (e.g. after
  // the renderer gets killed in background on Android).
  controller.SetNeedsReload();
  ASSERT_TRUE(controller.NeedsReload());
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      controller.GetLastCommittedEntry()->GetTransitionType(),
      ui::PAGE_TRANSITION_RELOAD));

  // Set the controller as active, triggering the requested reload.
  controller.SetActive(true);
  ASSERT_FALSE(controller.NeedsReload());
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      controller.GetPendingEntry()->GetTransitionType(),
      ui::PAGE_TRANSITION_RELOAD));
}

// Test requesting and triggering a lazy reload when there's only the initial
// entry.
TEST_F(NavigationControllerTest, LazyReloadWithOnlyInitialEntry) {
  NavigationControllerImpl& controller = controller_impl();
  ASSERT_TRUE(controller.GetLastCommittedEntry()->IsInitialEntry());
  EXPECT_FALSE(controller.NeedsReload());
  controller.SetNeedsReload();
  EXPECT_TRUE(controller.NeedsReload());

  // Doing a "load if necessary" shouldn't DCHECK.
  controller.LoadIfNecessary();
  ASSERT_FALSE(controller.NeedsReload());
}

// Test requesting and triggering a lazy reload when there's only the initial
// entry and a pending entry.
TEST_F(NavigationControllerTest, LazyReloadWithOnlyInitialAndPendingEntry) {
  NavigationControllerImpl& controller = controller_impl();
  const GURL url("http://foo");
  controller.LoadURL(url, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  ASSERT_FALSE(controller.NeedsReload());
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      controller.GetPendingEntry()->GetTransitionType(),
      ui::PAGE_TRANSITION_TYPED));

  // Request a reload to happen when the controller becomes active (e.g. after
  // the renderer gets killed in background on Android).
  controller.SetNeedsReload();
  ASSERT_TRUE(controller.NeedsReload());
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      controller.GetPendingEntry()->GetTransitionType(),
      ui::PAGE_TRANSITION_TYPED));

  // Set the controller as active, triggering the requested reload.
  controller.SetActive(true);
  ASSERT_FALSE(controller.NeedsReload());
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      controller.GetPendingEntry()->GetTransitionType(),
      ui::PAGE_TRANSITION_TYPED));
}

// Verify that a subframe navigation happening during an ongoing main frame
// navigation does not change the displayed URL.
// Original bug: http://crbug.com/43967
TEST_F(NavigationControllerTest, SubframeWhilePending) {
  NavigationControllerImpl& controller = controller_impl();
  // Load the first page.
  const GURL url1("http://foo/");
  NavigateAndCommit(url1);

  // Now start a load to a totally different page, but don't commit it.
  const GURL url2("http://bar/");
  auto main_frame_navigation =
      NavigationSimulator::CreateBrowserInitiated(url2, contents());
  main_frame_navigation->Start();

  // Navigate a subframe.
  std::string unique_name("uniqueName0");
  main_test_rfh()->OnCreateChildFrame(
      process()->GetNextRoutingID(),
      TestRenderFrameHost::CreateStubFrameRemote(),
      TestRenderFrameHost::CreateStubBrowserInterfaceBrokerReceiver(),
      TestRenderFrameHost::CreateStubPolicyContainerBindParams(),
      TestRenderFrameHost::CreateStubAssociatedInterfaceProviderReceiver(),
      blink::mojom::TreeScopeType::kDocument, std::string(), unique_name, false,
      blink::LocalFrameToken(), base::UnguessableToken::Create(),
      blink::DocumentToken(), blink::FramePolicy(),
      blink::mojom::FrameOwnerProperties(),
      blink::FrameOwnerElementType::kIframe, ukm::kInvalidSourceId);
  TestRenderFrameHost* subframe =
      static_cast<TestRenderFrameHost*>(contents()
                                            ->GetPrimaryFrameTree()
                                            .root()
                                            ->child_at(0)
                                            ->current_frame_host());
  const GURL url1_sub("http://foo/subframe");

  auto subframe_navigation =
      NavigationSimulator::CreateRendererInitiated(url1_sub, subframe);
  subframe_navigation->Start();

  // Creating the subframe navigation should have no effect on the pending
  // navigation entry and on the visible URL.
  EXPECT_EQ(url1, controller.GetLastCommittedEntry()->GetURL());
  EXPECT_EQ(url2, controller.GetVisibleEntry()->GetURL());

  auto nav_entry_id = [](NavigationSimulator* simulator) {
    return NavigationRequest::From(simulator->GetNavigationHandle())
        ->nav_entry_id();
  };

  // The main frame navigation is still associated with the pending entry, the
  // subframe one isn't.
  EXPECT_EQ(controller.GetPendingEntry()->GetUniqueID(),
            nav_entry_id(main_frame_navigation.get()));
  EXPECT_EQ(0, nav_entry_id(subframe_navigation.get()));

  subframe_navigation->Commit();

  // The subframe navigation should have no effect on the displayed url.
  EXPECT_EQ(url1, controller.GetLastCommittedEntry()->GetURL());
  EXPECT_EQ(url2, controller.GetVisibleEntry()->GetURL());

  EXPECT_EQ(controller.GetPendingEntry()->GetUniqueID(),
            nav_entry_id(main_frame_navigation.get()));
}

// Test CopyStateFrom with 2 urls, the first selected and nothing in the target.
TEST_F(NavigationControllerTest, CopyStateFrom) {
  NavigationControllerImpl& controller = controller_impl();
  const GURL url1("http://foo1");
  const GURL url2("http://foo2");

  NavigateAndCommit(url1);
  NavigateAndCommit(url2);
  controller.GoBack();
  contents()->CommitPendingNavigation();

  std::unique_ptr<TestWebContents> other_contents(
      static_cast<TestWebContents*>(CreateTestWebContents().release()));
  NavigationControllerImpl& other_controller = other_contents->GetController();

  // Before copying state, the new FrameTree's root is marked as "on the
  // initial empty document".
  EXPECT_TRUE(
      other_controller.frame_tree().root()->is_on_initial_empty_document());

  other_controller.CopyStateFrom(&controller, /*needs_reload=*/true);

  // After copying state, the new FrameTree's root is still marked as "on the
  // initial empty document".
  EXPECT_TRUE(
      other_controller.frame_tree().root()->is_on_initial_empty_document());

  // other_controller should now contain 2 urls.
  ASSERT_EQ(2, other_controller.GetEntryCount());
  // We should be looking at the first one.
  ASSERT_EQ(0, other_controller.GetCurrentEntryIndex());

  EXPECT_EQ(url1, other_controller.GetEntryAtIndex(0)->GetURL());
  EXPECT_EQ(url2, other_controller.GetEntryAtIndex(1)->GetURL());

  // Ensure the SessionStorageNamespaceMaps are the same size and have
  // the same partitons loaded.
  //
  // TODO(ajwong): We should load a url from a different partition earlier
  // to make sure this map has more than one entry.
  const SessionStorageNamespaceMap& session_storage_namespace_map =
      controller.GetSessionStorageNamespaceMap();
  const SessionStorageNamespaceMap& other_session_storage_namespace_map =
      other_controller.GetSessionStorageNamespaceMap();
  EXPECT_EQ(session_storage_namespace_map.size(),
            other_session_storage_namespace_map.size());
  for (auto it = session_storage_namespace_map.begin();
       it != session_storage_namespace_map.end(); ++it) {
    auto other = other_session_storage_namespace_map.find(it->first);
    EXPECT_TRUE(other != other_session_storage_namespace_map.end());
  }
}

// Tests DeleteNavigationEntries.
TEST_F(NavigationControllerTest, DeleteNavigationEntries) {
  NavigationControllerImpl& controller = controller_impl();

  const GURL url1("http://foo/1");
  const GURL url2("http://foo/2");
  const GURL url3("http://foo/3");
  const GURL url4("http://foo/4");
  const GURL url5("http://foo/5");

  NavigateAndCommit(url1);
  NavigateAndCommit(url2);
  NavigateAndCommit(url3);
  NavigateAndCommit(url4);
  NavigateAndCommit(url5);

  // Delete nothing.
  controller.DeleteNavigationEntries(base::BindRepeating(
      [](content::NavigationEntry* entry) { return false; }));
  EXPECT_EQ(0U, navigation_entries_deleted_counter_);
  ASSERT_EQ(5, controller.GetEntryCount());
  ASSERT_EQ(4, controller.GetCurrentEntryIndex());

  {
    // Delete url2 and url4.
    testing::NiceMock<MockPageBroadcast> mock_page_broadcast;
    contents()->GetRenderViewHost()->BindPageBroadcast(
        mock_page_broadcast.GetRemote());
    EXPECT_CALL(mock_page_broadcast, SetHistoryOffsetAndLength(2, 3));
    controller.DeleteNavigationEntries(
        base::BindLambdaForTesting([&](content::NavigationEntry* entry) {
          return entry->GetURL() == url2 || entry->GetURL() == url4;
        }));
    EXPECT_EQ(1U, navigation_entries_deleted_counter_);
    ASSERT_EQ(3, controller.GetEntryCount());
    ASSERT_EQ(2, controller.GetCurrentEntryIndex());
    EXPECT_EQ(url1, controller.GetEntryAtIndex(0)->GetURL());
    EXPECT_EQ(url3, controller.GetEntryAtIndex(1)->GetURL());
    EXPECT_EQ(url5, controller.GetEntryAtIndex(2)->GetURL());
    EXPECT_TRUE(controller.CanGoBack());
  }

  {
    // Delete url1 and url3.
    testing::NiceMock<MockPageBroadcast> mock_page_broadcast;
    contents()->GetRenderViewHost()->BindPageBroadcast(
        mock_page_broadcast.GetRemote());
    EXPECT_CALL(mock_page_broadcast, SetHistoryOffsetAndLength(0, 1));
    controller.DeleteNavigationEntries(base::BindRepeating(
        [](content::NavigationEntry* entry) { return true; }));
    EXPECT_EQ(2U, navigation_entries_deleted_counter_);
    ASSERT_EQ(1, controller.GetEntryCount());
    ASSERT_EQ(0, controller.GetCurrentEntryIndex());
    EXPECT_EQ(url5, controller.GetEntryAtIndex(0)->GetURL());
    EXPECT_FALSE(controller.CanGoBack());
  }

  // No pruned notifications should be send.
  EXPECT_EQ(0U, navigation_list_pruned_counter_);
}

// Tests that navigations initiated from the page (with the history object)
// work as expected, creating pending entries.
TEST_F(NavigationControllerTest, HistoryNavigate) {
  NavigationControllerImpl& controller = controller_impl();
  const GURL url1("http://foo/1");
  const GURL url2("http://foo/2");
  const GURL url3("http://foo/3");

  NavigateAndCommit(url1);
  NavigateAndCommit(url2);
  NavigateAndCommit(url3);
  controller.GoBack();
  contents()->CommitPendingNavigation();
  process()->sink().ClearMessages();

  // Simulate the page calling history.back(). It should create a pending entry.
  main_test_rfh()->GoToEntryAtOffset(-1, false, std::nullopt);
  EXPECT_EQ(0, controller.GetPendingEntryIndex());

  // Also make sure we told the page to navigate.
  GURL nav_url = GetLastNavigationURL();
  EXPECT_EQ(url1, nav_url);
  contents()->CommitPendingNavigation();
  process()->sink().ClearMessages();

  // Now test history.forward()
  main_test_rfh()->GoToEntryAtOffset(2, false, std::nullopt);
  EXPECT_EQ(2, controller.GetPendingEntryIndex());

  nav_url = GetLastNavigationURL();
  EXPECT_EQ(url3, nav_url);
  contents()->CommitPendingNavigation();
  process()->sink().ClearMessages();

  controller.DiscardNonCommittedEntries();

  // Make sure an extravagant history.go() doesn't break.
  main_test_rfh()->GoToEntryAtOffset(120, false,
                                     std::nullopt);  // Out of bounds.
  EXPECT_EQ(-1, controller.GetPendingEntryIndex());
  // TODO(crbug.com/40780539): Figure out why HasNavigationRequest() is
  // true when back/forward cache is enabled.
  EXPECT_EQ(IsBackForwardCacheEnabled(), HasNavigationRequest());
}

// Test call to PruneAllButLastCommitted for the only entry.
TEST_F(NavigationControllerTest, PruneAllButLastCommittedForSingle) {
  NavigationControllerImpl& controller = controller_impl();
  const GURL url1("http://foo1");
  NavigateAndCommit(url1);

  testing::NiceMock<MockPageBroadcast> mock_page_broadcast;
  contents()->GetRenderViewHost()->BindPageBroadcast(
      mock_page_broadcast.GetRemote());
  EXPECT_CALL(mock_page_broadcast, SetHistoryOffsetAndLength(0, 1));
  controller.PruneAllButLastCommitted();

  EXPECT_EQ(-1, controller.GetPendingEntryIndex());
  EXPECT_EQ(controller.GetEntryAtIndex(0)->GetURL(), url1);
}

// Test call to PruneAllButLastCommitted for first entry.
TEST_F(NavigationControllerTest, PruneAllButLastCommittedForFirst) {
  if (IsBackForwardCacheEnabled()) {
    // The PruneAllButLastCommitted() call below currently DCHECKS on the
    // linux-bfcache-rel bot with back/forward cache enabled because
    // CanPruneAllButLastCommitted() returns false, so just return early here.
    // TODO(crbug.com/40780539): Figure out why the DCHECK happened and
    // re-enable this test.
    return;
  }

  NavigationControllerImpl& controller = controller_impl();
  const GURL url1("http://foo/1");
  const GURL url2("http://foo/2");
  const GURL url3("http://foo/3");

  NavigateAndCommit(url1);
  NavigateAndCommit(url2);
  NavigateAndCommit(url3);
  controller.GoBack();
  controller.GoBack();
  contents()->CommitPendingNavigation();

  testing::NiceMock<MockPageBroadcast> mock_page_broadcast;
  contents()->GetRenderViewHost()->BindPageBroadcast(
      mock_page_broadcast.GetRemote());
  EXPECT_CALL(mock_page_broadcast, SetHistoryOffsetAndLength(0, 1));

  controller.PruneAllButLastCommitted();

  EXPECT_EQ(-1, controller.GetPendingEntryIndex());
  EXPECT_EQ(controller.GetEntryAtIndex(0)->GetURL(), url1);
}

// Test call to PruneAllButLastCommitted for intermediate entry.
TEST_F(NavigationControllerTest, PruneAllButLastCommittedForIntermediate) {
  NavigationControllerImpl& controller = controller_impl();
  const GURL url1("http://foo/1");
  const GURL url2("http://foo/2");
  const GURL url3("http://foo/3");

  NavigateAndCommit(url1);
  NavigateAndCommit(url2);
  NavigateAndCommit(url3);
  controller.GoBack();
  contents()->CommitPendingNavigation();

  testing::NiceMock<MockPageBroadcast> mock_page_broadcast;
  contents()->GetRenderViewHost()->BindPageBroadcast(
      mock_page_broadcast.GetRemote());
  EXPECT_CALL(mock_page_broadcast, SetHistoryOffsetAndLength(0, 1));

  controller.PruneAllButLastCommitted();

  EXPECT_EQ(-1, controller.GetPendingEntryIndex());
  EXPECT_EQ(controller.GetEntryAtIndex(0)->GetURL(), url2);
}

// Test call to PruneAllButLastCommitted for a pending entry that is not yet in
// the list of entries.
TEST_F(NavigationControllerTest, PruneAllButLastCommittedForPendingNotInList) {
  NavigationControllerImpl& controller = controller_impl();
  const GURL url1("http://foo/1");
  const GURL url2("http://foo/2");
  const GURL url3("http://foo/3");

  NavigateAndCommit(url1);
  NavigateAndCommit(url2);

  // Create a pending entry that is not in the entry list.
  auto navigation =
      NavigationSimulator::CreateBrowserInitiated(url3, contents());
  navigation->Start();
  EXPECT_TRUE(controller.GetPendingEntry());
  EXPECT_EQ(2, controller.GetEntryCount());

  {
    // Ensure that the PruneAllButLastCommitted() call will result in a
    // SetHistoryOffsetAndLength() call. We put this into its own scope so that
    // other PageBroadcast calls (e.g. SetPageLifecycleState()) won't go through
    // the mock.
    testing::NiceMock<MockPageBroadcast> mock_page_broadcast;
    contents()->GetRenderViewHost()->BindPageBroadcast(
        mock_page_broadcast.GetRemote());
    EXPECT_CALL(mock_page_broadcast, SetHistoryOffsetAndLength(0, 1));
    controller.PruneAllButLastCommitted();
  }

  // We should only have the last committed and pending entries at this point,
  // and the pending entry should still not be in the entry list.
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(url2, controller.GetEntryAtIndex(0)->GetURL());
  EXPECT_EQ(-1, controller.GetPendingEntryIndex());
  EXPECT_TRUE(controller.GetPendingEntry());
  EXPECT_EQ(1, controller.GetEntryCount());

  // Try to commit the pending entry.
  navigation->Commit();
  EXPECT_EQ(-1, controller.GetPendingEntryIndex());
  EXPECT_FALSE(controller.GetPendingEntry());
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(url3, controller.GetEntryAtIndex(1)->GetURL());
}

// Test to ensure that when we do a history navigation back to the current
// committed page (e.g., going forward to a slow-loading page, then pressing
// the back button), we just stop the navigation to prevent the throbber from
// running continuously. Otherwise, the RenderViewHost forces the throbber to
// start, but WebKit essentially ignores the navigation and never sends a
// message to stop the throbber.
TEST_F(NavigationControllerTest, StopOnHistoryNavigationToCurrentPage) {
  NavigationControllerImpl& controller = controller_impl();
  const GURL url0("http://foo/0");
  const GURL url1("http://foo/1");

  NavigateAndCommit(url0);
  NavigateAndCommit(url1);

  // Go back to the original page, then forward to the slow page, then back
  controller.GoBack();
  contents()->CommitPendingNavigation();

  controller.GoForward();
  EXPECT_EQ(1, controller.GetPendingEntryIndex());

  controller.GoBack();
  EXPECT_EQ(-1, controller.GetPendingEntryIndex());
}

TEST_F(NavigationControllerTest, IsInitialNavigation) {
  NavigationControllerImpl& controller = controller_impl();

  // Initial state.
  EXPECT_TRUE(controller.IsInitialNavigation());
  EXPECT_TRUE(controller.IsInitialBlankNavigation());

  // After commit, it stays false.
  const GURL url1("http://foo1");
  NavigationSimulator::NavigateAndCommitFromDocument(url1, main_test_rfh());
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;
  EXPECT_FALSE(controller.IsInitialNavigation());
  EXPECT_FALSE(controller.IsInitialBlankNavigation());

  // After starting a new navigation, it stays false.
  const GURL url2("http://foo2");
  controller.LoadURL(url2, Referrer(), ui::PAGE_TRANSITION_TYPED,
                     std::string());
  EXPECT_FALSE(controller.IsInitialNavigation());
  EXPECT_FALSE(controller.IsInitialBlankNavigation());

  // For cloned tabs, IsInitialNavigationShould be true but
  // IsInitialBlankNavigation should be false.
  std::unique_ptr<WebContents> clone(
      RenderViewHostTestHarness::web_contents()->Clone());
  EXPECT_TRUE(clone->GetController().IsInitialNavigation());
  EXPECT_FALSE(clone->GetController().IsInitialBlankNavigation());
}

// Check that the favicon is not reused across a client redirect.
// (crbug.com/28515)
TEST_F(NavigationControllerTest, ClearFaviconOnRedirect) {
  const GURL kPageWithFavicon("http://withfavicon.html");
  const GURL kPageWithoutFavicon("http://withoutfavicon.html");
  const GURL kIconURL("http://withfavicon.ico");
  const gfx::Image kDefaultFavicon = FaviconStatus().image;

  NavigationControllerImpl& controller = controller_impl();

  NavigationSimulator::NavigateAndCommitFromDocument(kPageWithFavicon,
                                                     main_test_rfh());
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;

  NavigationEntry* entry = controller.GetLastCommittedEntry();
  EXPECT_TRUE(entry);
  EXPECT_EQ(kPageWithFavicon, entry->GetURL());

  // Simulate Chromium having set the favicon for |kPageWithFavicon|.
  content::FaviconStatus& favicon_status = entry->GetFavicon();
  favicon_status.image = CreateImage(SK_ColorWHITE);
  favicon_status.url = kIconURL;
  favicon_status.valid = true;
  EXPECT_FALSE(DoImagesMatch(kDefaultFavicon, entry->GetFavicon().image));

  std::unique_ptr<NavigationSimulator> simulator =
      NavigationSimulator::CreateRendererInitiated(kPageWithoutFavicon,
                                                   main_test_rfh());
  simulator->SetTransition(ui::PAGE_TRANSITION_CLIENT_REDIRECT);
  simulator->Commit();
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;

  entry = controller.GetLastCommittedEntry();
  EXPECT_TRUE(entry);
  EXPECT_EQ(kPageWithoutFavicon, entry->GetURL());

  EXPECT_TRUE(DoImagesMatch(kDefaultFavicon, entry->GetFavicon().image));
}

// Check that the favicon is not cleared for NavigationEntries which were
// previously navigated to.
TEST_F(NavigationControllerTest, BackNavigationDoesNotClearFavicon) {
  const GURL kUrl1("http://www.a.com/1");
  const GURL kUrl2("http://www.a.com/2");
  const GURL kIconURL("http://www.a.com/1/favicon.ico");

  NavigationControllerImpl& controller = controller_impl();

  NavigationSimulator::NavigateAndCommitFromDocument(kUrl1, main_test_rfh());
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;

  // Simulate Chromium having set the favicon for |kUrl1|.
  gfx::Image favicon_image = CreateImage(SK_ColorWHITE);
  content::NavigationEntry* entry = controller.GetLastCommittedEntry();
  EXPECT_TRUE(entry);
  content::FaviconStatus& favicon_status = entry->GetFavicon();
  favicon_status.image = favicon_image;
  favicon_status.url = kIconURL;
  favicon_status.valid = true;

  // Navigate to another page and go back to the original page.
  NavigationSimulator::NavigateAndCommitFromDocument(kUrl2, main_test_rfh());
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;

  NavigationSimulator::GoBack(contents());
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;

  // Verify that the favicon for the page at |kUrl1| was not cleared.
  entry = controller.GetEntryAtIndex(0);
  EXPECT_TRUE(entry);
  EXPECT_EQ(kUrl1, entry->GetURL());
  EXPECT_TRUE(DoImagesMatch(favicon_image, entry->GetFavicon().image));
}

TEST_F(NavigationControllerTest, PushStateUpdatesTitleAndFavicon) {
  // Navigate.
  NavigationSimulator::NavigateAndCommitFromDocument(GURL("http://foo"),
                                                     main_test_rfh());

  // Set title and favicon.
  std::u16string title(u"Title");
  FaviconStatus favicon;
  favicon.valid = true;
  favicon.url = GURL("http://foo/favicon.ico");
  contents()->UpdateTitleForEntry(controller().GetLastCommittedEntry(), title);
  controller().GetLastCommittedEntry()->GetFavicon() = favicon;

  // history.pushState() is called.
  auto params = mojom::DidCommitProvisionalLoadParams::New();
  GURL kUrl2("http://foo#foo");
  params->did_create_new_entry = true;
  params->url = kUrl2;
  params->referrer = blink::mojom::Referrer::New();
  params->page_state = blink::PageState::CreateFromURL(kUrl2);
  params->method = "GET";
  params->should_update_history = true;
  params->post_id = -1;
  main_test_rfh()->SendNavigateWithParams(std::move(params), true);

  // The title should immediately be visible on the new NavigationEntry.
  std::u16string new_title =
      controller().GetLastCommittedEntry()->GetTitleForDisplay();
  EXPECT_EQ(title, new_title);
  FaviconStatus new_favicon =
      controller().GetLastCommittedEntry()->GetFavicon();
  EXPECT_EQ(favicon.valid, new_favicon.valid);
  EXPECT_EQ(favicon.url, new_favicon.url);
}

// Test that the navigation controller clears its session history when a
// navigation commits with the clear history list flag set.
TEST_F(NavigationControllerTest, ClearHistoryList) {
  const GURL url1("http://foo1");
  const GURL url2("http://foo2");
  const GURL url3("http://foo3");
  const GURL url4("http://foo4");

  NavigationControllerImpl& controller = controller_impl();

  // Create a session history with three entries, second entry is active.
  NavigateAndCommit(url1);
  NavigateAndCommit(url2);
  NavigateAndCommit(url3);
  NavigationSimulator::GoBack(contents());
  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetCurrentEntryIndex());

  // Create a new pending navigation, and indicate that the session history
  // should be cleared.
  auto navigation =
      NavigationSimulatorImpl::CreateBrowserInitiated(url4, contents());
  NavigationController::LoadURLParams load_url_params(url4);
  load_url_params.should_clear_history_list = true;
  navigation->SetLoadURLParams(&load_url_params);
  navigation->Start();

  // Verify that the pending entry correctly indicates that the session history
  // should be cleared.
  NavigationEntryImpl* entry = controller.GetPendingEntry();
  ASSERT_TRUE(entry);
  EXPECT_TRUE(entry->should_clear_history_list());

  // Assume that the RenderFrame correctly cleared its history and commit the
  // navigation.
  navigation->set_history_list_was_cleared(true);
  navigation->Commit();

  // Verify that the NavigationController's session history was correctly
  // cleared.
  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(0, controller.GetCurrentEntryIndex());
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(-1, controller.GetPendingEntryIndex());
  EXPECT_FALSE(controller.CanGoBack());
  EXPECT_FALSE(controller.CanGoForward());
  EXPECT_EQ(url4, controller.GetVisibleEntry()->GetURL());
}

// Tests that if a stale navigation comes back from the renderer, it is properly
// resurrected.
TEST_F(NavigationControllerTest, StaleNavigationsResurrected) {
  if (ShouldQueueNavigationsWhenPendingCommitRFHExists()) {
    GTEST_SKIP() << "When navigation queueing is enabled, there will be no "
                    "stale navigations, as newer navigations will wait for "
                    "pending commit navigations to finish";
  }

  NavigationControllerImpl& controller = controller_impl();
  // When back/forward cache is enabled, the ReadyToCommit() call for the
  // forward navigation to B will commit the navigation immediately, making the
  // navigation to page C not prune the entry to B. Disable back/forward cache
  // to ensure that it doesn't get preserved in the cache.
  DisableBackForwardCacheForTesting(RenderViewHostTestHarness::web_contents(),
                                    BackForwardCache::TEST_REQUIRES_NO_CACHING);
  // Start on page A.
  const GURL url_a("http://foo.com/a");
  NavigationSimulator::NavigateAndCommitFromDocument(url_a, main_test_rfh());
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;
  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(0, controller.GetCurrentEntryIndex());

  // Go to page B.
  const GURL url_b("http://foo.com/b");
  NavigationSimulator::NavigateAndCommitFromDocument(url_b, main_test_rfh());
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetCurrentEntryIndex());
  int b_entry_id = controller.GetLastCommittedEntry()->GetUniqueID();

  // Back to page A.
  NavigationSimulator::GoBack(contents());
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(0, controller.GetCurrentEntryIndex());

  // Start going forward to page B.
  auto forward_navigation = NavigationSimulator::CreateHistoryNavigation(
      1, contents(), false /* is_renderer_initiated */);
  forward_navigation->ReadyToCommit();

  // But the renderer unilaterally navigates to page C, pruning B.
  const GURL url_c("http://foo.com/c");
  NavigationSimulator::NavigateAndCommitFromDocument(url_c, main_test_rfh());
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetCurrentEntryIndex());
  int c_entry_id = controller.GetLastCommittedEntry()->GetUniqueID();
  EXPECT_NE(c_entry_id, b_entry_id);

  // And then the navigation to B gets committed.
  forward_navigation->Commit();
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;

  // Even though we were doing a history navigation, because the entry was
  // pruned it will end up as a *new* entry at the end of the entry list. This
  // means that occasionally a navigation conflict will end up with one entry
  // bubbling to the end of the entry list, but that's the least-bad option.
  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(2, controller.GetCurrentEntryIndex());
  EXPECT_EQ(url_a, controller.GetEntryAtIndex(0)->GetURL());
  EXPECT_EQ(url_c, controller.GetEntryAtIndex(1)->GetURL());
  EXPECT_EQ(url_b, controller.GetEntryAtIndex(2)->GetURL());
}

// Tests that successive navigations with intermittent duplicate navigations
// are correctly marked as reload in the navigation controller.
// We test the cases where in a navigation is pending/committed before the new
// navigation is initiated.
// http://crbug.com/664319
TEST_F(NavigationControllerTest, MultipleNavigationsAndReload) {
  NavigationControllerImpl& controller = controller_impl();

  GURL initial_url("http://www.google.com");
  GURL url_1("http://foo.com");
  GURL url_2("http://foo2.com");

  // Test 1.
  // A normal navigation to initial_url should not be marked as a reload.
  auto navigation1 =
      NavigationSimulator::CreateBrowserInitiated(initial_url, contents());
  navigation1->Start();
  EXPECT_EQ(initial_url, controller.GetVisibleEntry()->GetURL());
  navigation1->Commit();
  EXPECT_EQ(ReloadType::NONE, last_reload_type_);
  EXPECT_EQ(ReloadType::NONE, main_test_rfh()->reload_type());

  // Test 2.
  // A navigation to initial_url with the navigation commit delayed should be
  // marked as a reload.
  auto navigation2 =
      NavigationSimulator::CreateBrowserInitiated(initial_url, contents());
  navigation2->Start();
  EXPECT_EQ(initial_url, controller.GetVisibleEntry()->GetURL());
  navigation2->ReadyToCommit();
  EXPECT_EQ(initial_url, controller.GetVisibleEntry()->GetURL());
  EXPECT_EQ(ReloadType::NORMAL, last_reload_type_);

  // Test 3.
  // A navigation to url_1 while the navigation to intial_url is still pending
  // should not be marked as a reload.
  auto navigation3 =
      NavigationSimulator::CreateBrowserInitiated(url_1, contents());
  navigation3->Start();
  EXPECT_EQ(url_1, controller.GetVisibleEntry()->GetURL());
  EXPECT_EQ(ReloadType::NONE, last_reload_type_);

  // Test 4.
  // A navigation to url_1 while the previous navigation to url_1 is pending
  // should not be marked as reload. Even though the URL is the same as the
  // previous navigation, the previous navigation did not commit. We can only
  // reload navigations that committed. See https://crbug.com/809040.
  auto navigation4 =
      NavigationSimulator::CreateBrowserInitiated(url_1, contents());
  navigation4->Start();
  EXPECT_EQ(url_1, controller.GetVisibleEntry()->GetURL());
  EXPECT_EQ(ReloadType::NONE, last_reload_type_);

  navigation2->Commit();
  EXPECT_EQ(ReloadType::NORMAL, main_test_rfh()->reload_type());  // from nav2.

  // Test 5
  // A navigation to url_2 followed by a navigation to the previously pending
  // url_1 should not be marked as a reload.
  auto navigation5 =
      NavigationSimulator::CreateBrowserInitiated(url_2, contents());
  navigation5->Start();
  EXPECT_EQ(url_2, controller.GetVisibleEntry()->GetURL());
  EXPECT_EQ(ReloadType::NONE, last_reload_type_);

  controller.LoadURL(url_1, Referrer(), ui::PAGE_TRANSITION_TYPED,
                     std::string());
  auto navigation6 =
      NavigationSimulator::CreateBrowserInitiated(url_1, contents());
  navigation6->ReadyToCommit();
  EXPECT_EQ(url_1, controller.GetVisibleEntry()->GetURL());
  EXPECT_EQ(ReloadType::NONE, last_reload_type_);
  navigation6->Commit();
  EXPECT_EQ(ReloadType::NONE, main_test_rfh()->reload_type());
}

// Tests that NavigationUIData has been passed to the NavigationHandle.
TEST_F(NavigationControllerTest, MainFrameNavigationUIData) {
  LoadCommittedDetailsObserver observer(contents());
  const GURL url1("http://foo1");

  auto navigation =
      NavigationSimulatorImpl::CreateBrowserInitiated(url1, contents());
  NavigationController::LoadURLParams load_url_params(url1);
  load_url_params.navigation_ui_data = std::make_unique<TestNavigationUIData>();
  navigation->SetLoadURLParams(&load_url_params);
  navigation->Commit();

  EXPECT_TRUE(observer.is_main_frame());
  EXPECT_TRUE(observer.has_navigation_ui_data());
}

// Tests that ReloadType has been passed to the NavigationHandle.
TEST_F(NavigationControllerTest, MainFrameNavigationReloadType) {
  LoadCommittedDetailsObserver observer(contents());
  const GURL url1("http://foo1");

  auto navigation =
      NavigationSimulatorImpl::CreateBrowserInitiated(url1, contents());
  NavigationController::LoadURLParams load_url_params(url1);
  load_url_params.reload_type = ReloadType::BYPASSING_CACHE;
  navigation->SetLoadURLParams(&load_url_params);
  navigation->Commit();

  EXPECT_TRUE(observer.is_main_frame());
  EXPECT_EQ(observer.reload_type(), ReloadType::BYPASSING_CACHE);
  EXPECT_EQ(ReloadType::BYPASSING_CACHE, main_test_rfh()->reload_type());
}

// Tests calling LoadURLParams with NavigationUIData and for a sub frame.
TEST_F(NavigationControllerTest, SubFrameNavigationUIData) {
  // Navigate to a page.
  const GURL url1("http://foo1");
  NavigationSimulator::NavigateAndCommitFromDocument(url1, main_test_rfh());
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;

  // Add a sub frame.
  std::string unique_name("uniqueName0");
  main_test_rfh()->OnCreateChildFrame(
      process()->GetNextRoutingID(),
      TestRenderFrameHost::CreateStubFrameRemote(),
      TestRenderFrameHost::CreateStubBrowserInterfaceBrokerReceiver(),
      TestRenderFrameHost::CreateStubPolicyContainerBindParams(),
      TestRenderFrameHost::CreateStubAssociatedInterfaceProviderReceiver(),
      blink::mojom::TreeScopeType::kDocument, std::string(), unique_name, false,
      blink::LocalFrameToken(), base::UnguessableToken::Create(),
      blink::DocumentToken(), blink::FramePolicy(),
      blink::mojom::FrameOwnerProperties(),
      blink::FrameOwnerElementType::kIframe, ukm::kInvalidSourceId);
  TestRenderFrameHost* subframe =
      static_cast<TestRenderFrameHost*>(contents()
                                            ->GetPrimaryFrameTree()
                                            .root()
                                            ->child_at(0)
                                            ->current_frame_host());
  const GURL subframe_url("http://foo1/subframe");

  LoadCommittedDetailsObserver observer(contents());

  // Navigate sub frame.
  NavigationController::LoadURLParams load_url_params(url1);
  load_url_params.navigation_ui_data = std::make_unique<TestNavigationUIData>();
  load_url_params.frame_tree_node_id = subframe->GetFrameTreeNodeId();

  auto navigation =
      NavigationSimulatorImpl::CreateBrowserInitiated(url1, contents());
  navigation->SetLoadURLParams(&load_url_params);

  // We DCHECK to prevent misuse of the API.
  EXPECT_DCHECK_DEATH(navigation->Start());
}

bool SrcDocRewriter(GURL* url, BrowserContext* browser_context) {
  if (url->IsAboutSrcdoc()) {
    *url = GURL("chrome://srcdoc");
    return true;
  }
  return false;
}

// Tests that receiving a request to navigate a subframe will not rewrite the
// subframe URL. Regression test for https://crbug.com/895065.
TEST_F(NavigationControllerTest, NoURLRewriteForSubframes) {
  const GURL kUrl1("http://google.com");
  const GURL kUrl2("http://chromium.org");
  const GURL kSrcDoc("about:srcdoc");

  // First, set up a handler that will rewrite srcdoc urls.
  BrowserURLHandlerImpl::GetInstance()->AddHandlerPair(
      &SrcDocRewriter, BrowserURLHandler::null_handler());

  // Simulate navigating to a page that has a subframe.
  NavigationSimulator::NavigateAndCommitFromDocument(kUrl1, main_test_rfh());
  TestRenderFrameHost* subframe = main_test_rfh()->AppendChild("subframe");
  NavigationSimulator::NavigateAndCommitFromDocument(kUrl2, subframe);

  // Simulate the subframe receiving a request from a RenderFrameProxyHost to
  // navigate to about:srcdoc. This should not crash.
  FrameTreeNode* subframe_node =
      main_test_rfh()->frame_tree_node()->child_at(0);
  controller_impl().NavigateFromFrameProxy(
      subframe_node->current_frame_host(), kSrcDoc,
      nullptr /* initiator_frame_token */,
      ChildProcessHost::kInvalidUniqueID /* initiator_process_id */,
      url::Origin::Create(kUrl2), /* initiator_base_url= */ std::nullopt,
      true /* is_renderer_initiated */, main_test_rfh()->GetSiteInstance(),
      Referrer(), ui::PAGE_TRANSITION_LINK,
      false /* should_replace_current_entry */,
      blink::NavigationDownloadPolicy(), "GET", nullptr, "",
      network::mojom::SourceLocation::New(), nullptr,
      false /*is_form_submission*/, std::nullopt,
      blink::mojom::NavigationInitiatorActivationAndAdStatus::
          kDidNotStartWithTransientActivation,
      base::TimeTicks::Now() /* navigation_start_time */);

  // Clean up the handler.
  BrowserURLHandlerImpl::GetInstance()->RemoveHandlerForTesting(
      &SrcDocRewriter);
}

// Test that if an empty WebContents is navigated via frame proxy with
// replacement, the NavigationRequest does specifies replacement, to replace the
// initial entry.
TEST_F(NavigationControllerTest,
       NavigateFromFrameProxyWithReplacementWithOnlyInitialEntry) {
  const GURL main_url("http://foo1");
  const GURL other_contents_url("http://foo2");
  NavigationSimulator::NavigateAndCommitFromBrowser(contents(), main_url);

  // Suppose the main WebContents creates another WebContents which it can
  // navigate via frame proxy.
  std::unique_ptr<WebContents> other_contents = CreateTestWebContents();
  TestWebContents* other_contents_impl =
      static_cast<TestWebContents*>(other_contents.get());
  NavigationControllerImpl& other_controller =
      other_contents_impl->GetController();
  FrameTreeNode* node = other_contents_impl->GetPrimaryFrameTree().root();
  RenderFrameHostImpl* frame = node->current_frame_host();

  // The newly created contents has 1 entry, the initial entry.
  EXPECT_EQ(1, other_controller.GetEntryCount());
  EXPECT_TRUE(other_controller.GetLastCommittedEntry()->IsInitialEntry());

  // Simulate the main WebContents navigating the new WebContents with
  // replacement.
  const bool should_replace_current_entry = true;
  other_controller.NavigateFromFrameProxy(
      frame, other_contents_url, nullptr /* initiator_frame_token */,
      ChildProcessHost::kInvalidUniqueID /* initiator_process_id */,
      url::Origin::Create(main_url), /* initiator_base_url= */ std::nullopt,
      true /* is_renderer_initiated */, main_test_rfh()->GetSiteInstance(),
      Referrer(), ui::PAGE_TRANSITION_LINK, should_replace_current_entry,
      blink::NavigationDownloadPolicy(), "GET", nullptr, "",
      network::mojom::SourceLocation::New(), nullptr,
      false /*is_form_submission*/, std::nullopt,
      blink::mojom::NavigationInitiatorActivationAndAdStatus::
          kDidNotStartWithTransientActivation,
      base::TimeTicks::Now() /* navigation_start_time */);
  NavigationRequest* request = node->navigation_request();
  ASSERT_TRUE(request);

  // The request was done with replacement.
  EXPECT_TRUE(request->common_params().should_replace_current_entry);
}

// Tests that calling RemoveForwareEntries() clears all forward entries
// including non-committed entries.
TEST_F(NavigationControllerTest, PruneForwardEntries) {
  NavigationControllerImpl& controller = controller_impl();
  const GURL url_0("http://foo/0");
  const GURL url_1("http://foo/1");
  const GURL url_2("http://foo/2");
  const GURL url_3("http://foo/3");

  NavigateAndCommit(url_0);
  NavigateAndCommit(url_1);
  NavigateAndCommit(url_2);
  NavigateAndCommit(url_3);

  // Set a WebContentsDelegate to listen for state changes.
  std::unique_ptr<TestWebContentsDelegate> delegate(
      new TestWebContentsDelegate());
  EXPECT_FALSE(contents()->GetDelegate());
  contents()->SetDelegate(delegate.get());

  controller.GoBack();

  // Ensure that non-committed entries are removed even if there are no forward
  // entries.
  EXPECT_EQ(4, controller.GetEntryCount());
  EXPECT_EQ(2, controller.GetPendingEntryIndex());
  EXPECT_EQ(2, controller.GetCurrentEntryIndex());
  EXPECT_EQ(3, controller.GetLastCommittedEntryIndex());
  int state_change_count = delegate->navigation_state_change_count();
  controller.PruneForwardEntries();
  EXPECT_EQ(4, controller.GetEntryCount());
  EXPECT_EQ(-1, controller.GetPendingEntryIndex());
  EXPECT_EQ(3, controller.GetCurrentEntryIndex());
  EXPECT_EQ(3, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(0U, navigation_list_pruned_counter_);
  EXPECT_EQ(state_change_count + 1, delegate->navigation_state_change_count());

  controller.GoBack();
  contents()->CommitPendingNavigation();
  controller.GoBack();
  contents()->CommitPendingNavigation();
  controller.GoBack();
  contents()->CommitPendingNavigation();
  controller.GoForward();

  EXPECT_EQ(1, controller.GetPendingEntryIndex());
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());

  state_change_count = delegate->navigation_state_change_count();
  controller.PruneForwardEntries();

  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_FALSE(controller.CanGoForward());
  EXPECT_FALSE(controller.CanGoBack());
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(0, controller.GetCurrentEntryIndex());
  EXPECT_EQ(-1, controller.GetPendingEntryIndex());
  EXPECT_EQ(nullptr, controller.GetPendingEntry());
  EXPECT_EQ(url_0, controller.GetVisibleEntry()->GetURL());
  EXPECT_EQ(1U, navigation_list_pruned_counter_);
  EXPECT_EQ(1, last_navigation_entry_pruned_details_.index);
  EXPECT_EQ(3, last_navigation_entry_pruned_details_.count);
  EXPECT_EQ(state_change_count + 1, delegate->navigation_state_change_count());
}

// Make sure that cloning a WebContentsImpl and clearing forward entries
// before the first commit doesn't clear all entries.
TEST_F(NavigationControllerTest, PruneForwardEntriesAfterClone) {
  const GURL url1("http://foo1");
  const GURL url2("http://foo2");

  NavigateAndCommit(url1);
  NavigateAndCommit(url2);

  std::unique_ptr<WebContents> clone(
      RenderViewHostTestHarness::web_contents()->Clone());
  clone->GetController().LoadIfNecessary();

  // Set a WebContentsDelegate to listen for state changes after the clone call
  // to only count state changes from the PruneForwardEntries call.
  std::unique_ptr<TestWebContentsDelegate> delegate(
      new TestWebContentsDelegate());
  EXPECT_FALSE(clone->GetDelegate());
  clone->SetDelegate(delegate.get());

  EXPECT_EQ(1, clone->GetController().GetPendingEntryIndex());

  clone->GetController().PruneForwardEntries();

  ASSERT_EQ(2, clone->GetController().GetEntryCount());
  EXPECT_EQ(-1, clone->GetController().GetPendingEntryIndex());
  EXPECT_EQ(url2, clone->GetController().GetVisibleEntry()->GetURL());
  EXPECT_EQ(0U, navigation_list_pruned_counter_);
  EXPECT_EQ(1, delegate->navigation_state_change_count());
}

TEST_F(NavigationControllerTest,
       NavigateToNavigationApiKey_DifferentSiteInstance) {
  NavigationControllerImpl& controller = controller_impl();
  const GURL url1("http://foo1");
  const GURL url2("http://foo2");

  // Navigate and set a key.
  NavigateAndCommit(url1);
  std::string first_key = "12345";
  controller.GetLastCommittedEntry()
      ->GetFrameEntry(root_ftn())
      ->set_navigation_api_key(first_key);

  // Navigate to a new site instance. The key should not be shared.
  NavigateAndCommit(url2);
  EXPECT_NE(controller.GetLastCommittedEntry()
                ->GetFrameEntry(root_ftn())
                ->navigation_api_key(),
            first_key);
  EXPECT_FALSE(controller.GetPendingEntry());

  // Attempte to provide the cross-site-instance key to
  // NavigateToNavigationApiKey(). No navigation should occur.
  controller.NavigateToNavigationApiKey(
      main_test_rfh(),
      /*soft_navigation_heuristics_task_id=*/std::nullopt, first_key);
  EXPECT_FALSE(controller.GetPendingEntry());
}

TEST_F(NavigationControllerTest, NavigateToNavigationApiKey_KeyForWrongFrame) {
  const GURL kUrl1("http://google.com");

  // Simulate navigating to a page that has a same-origin subframe.
  NavigationSimulator::NavigateAndCommitFromDocument(kUrl1, main_test_rfh());
  TestRenderFrameHost* subframe = main_test_rfh()->AppendChild("subframe");
  NavigationSimulator::NavigateAndCommitFromDocument(kUrl1, subframe);

  // Set a main frame key
  std::string first_main_key = "12345";
  controller_impl()
      .GetLastCommittedEntry()
      ->GetFrameEntry(root_ftn())
      ->set_navigation_api_key(first_main_key);

  // Navigate both frames again.
  const GURL kUrl2("http://google.com#bar");
  auto same_document_navigation_main =
      NavigationSimulator::CreateRendererInitiated(kUrl2, main_test_rfh());
  same_document_navigation_main->CommitSameDocument();
  auto same_document_navigation_subframe =
      NavigationSimulator::CreateRendererInitiated(kUrl2, subframe);
  same_document_navigation_subframe->CommitSameDocument();
  ASSERT_EQ(3, controller_impl().GetEntryCount());

  // Call NavigateToNavigationApiKey() on the subframe with the key from the
  // main frame. No navigation should begin, because we should only match keys
  // for the target frame.
  FrameTreeNode* subframe_node =
      main_test_rfh()->frame_tree_node()->child_at(0);
  controller_impl().NavigateToNavigationApiKey(
      subframe_node->current_frame_host(),
      /*soft_navigation_heuristics_task_id=*/std::nullopt, first_main_key);
  EXPECT_FALSE(controller_impl().GetPendingEntry());

  // Call NavigateToNavigationApiKey() on the main frame with the key from the
  // main frame. This time a navigation should begin.
  controller_impl().NavigateToNavigationApiKey(
      main_test_rfh(), /*soft_navigation_heuristics_task_id=*/std::nullopt,
      first_main_key);
  EXPECT_TRUE(controller_impl().GetPendingEntry());
}

class FakeLocalFrameWithDisposedEntries : public content::FakeLocalFrame {
 public:
  explicit FakeLocalFrameWithDisposedEntries(RenderFrameHost* host) {
    auto* test_host = static_cast<TestRenderFrameHost*>(host);
    test_host->ResetLocalFrame();
    Init(test_host->GetRemoteAssociatedInterfaces());
  }

  const std::vector<std::string>& disposed_keys() const {
    return disposed_keys_;
  }

  // FakeLocalFrame:
  void NotifyNavigationApiOfDisposedEntries(
      const std::vector<std::string>& keys) final {
    disposed_keys_ = keys;
  }

 private:
  std::vector<std::string> disposed_keys_;
};

TEST_F(NavigationControllerTest, NavigationApiDisposedEntries) {
  // Construct first entry (cross-origin to the rest).
  const GURL kUrl1("http://google.com");
  NavigationSimulator::NavigateAndCommitFromDocument(kUrl1, main_test_rfh());
  controller_impl()
      .GetLastCommittedEntry()
      ->GetFrameEntry(root_ftn())
      ->set_navigation_api_key("1");

  // Construct second entry (from which we will conduct the final checks).
  const GURL kUrl2("http://example.com");
  NavigationSimulator::NavigateAndCommitFromDocument(kUrl2, main_test_rfh());
  controller_impl()
      .GetLastCommittedEntry()
      ->GetFrameEntry(root_ftn())
      ->set_navigation_api_key("2");
  const GURL kSubframeUrl2("http://example.com/subframe");
  TestRenderFrameHost* subframe2 = main_test_rfh()->AppendChild("subframe");
  NavigationSimulator::NavigateAndCommitFromDocument(kSubframeUrl2, subframe2);
  controller_impl()
      .GetLastCommittedEntry()
      ->GetFrameEntry(root_ftn()->child_at(0))
      ->set_navigation_api_key("2_sub");

  std::string iframe_unique_name = root_ftn()->child_at(0)->unique_name();

  // Setup a FakeLocalFrame in order to inspect the
  // NotifyNavigationApiOfDisposedEntries() callback.
  FakeLocalFrameWithDisposedEntries main_frame(main_test_rfh());
  FakeLocalFrameWithDisposedEntries sub_frame(
      root_ftn()->child_at(0)->current_frame_host());
  main_test_rfh()->FlushLocalFrameMessages();
  static_cast<TestRenderFrameHost*>(
      root_ftn()->child_at(0)->current_frame_host())
      ->FlushLocalFrameMessages();

  // Construct third entry (same-origin to the second, but cross-document so
  // that the iframe gets detached). Set the iframe's FNE's unique name to match
  // the previous detached one, to ensure they aren't spuriously considered the
  // same conceptual iframe.
  const GURL kUrl3("http://example.com?next");
  NavigationSimulator::NavigateAndCommitFromDocument(kUrl3, main_test_rfh());
  controller_impl()
      .GetLastCommittedEntry()
      ->GetFrameEntry(root_ftn())
      ->set_navigation_api_key("3");
  const GURL kSubframeUrl3("http://example.com/subframe?next");
  TestRenderFrameHost* subframe3 = main_test_rfh()->AppendChild("subframe");
  NavigationSimulator::NavigateAndCommitFromDocument(kSubframeUrl3, subframe3);
  controller_impl()
      .GetLastCommittedEntry()
      ->GetFrameEntry(root_ftn()->child_at(0))
      ->set_navigation_api_key("3_sub");
  controller_impl()
      .GetLastCommittedEntry()
      ->GetFrameEntry(root_ftn()->child_at(0))
      ->set_frame_unique_name(iframe_unique_name);

  // Go back to the second entry.
  NavigationSimulator::GoBack(contents());
  EXPECT_EQ(controller_impl().GetLastCommittedEntryIndex(), 1);
  EXPECT_EQ(controller_impl().GetEntryCount(), 3);

  // Prune the first and last entries. In the main frame, we should notify the
  // renderer of the last key's disposal, but not the first key, because it was
  // cross-origin. In the subframe, we should not notify for either: the first
  // because the subframe was not present at all, and the second because the
  // subframe was detached and reattached, so it's not really the same frame,
  // even though it had the same unique name.
  controller_impl().PruneAllButLastCommitted();
  main_frame.FlushMessages();
  sub_frame.FlushMessages();
  EXPECT_EQ(sub_frame.disposed_keys().size(), 0u);

  auto main_frame_disposed_keys = main_frame.disposed_keys();
  EXPECT_EQ(main_frame_disposed_keys.size(), 1u);
  EXPECT_EQ(main_frame_disposed_keys[0], "3");
}

// Once instantiated, will insert `mock_page_broadcast` as the PageBroadcast on
// a newly created RenderViewHost. This is important for listening for the
// update to a RenderViewHost which was created for a proxy, as it swaps to a
// local frame in a different browsing context group. Note that this this will
// only work once, as MockPageBroadcast does not support multiple bindings.
class PageBroadcastMockInserter : public WebContentsObserver {
 public:
  explicit PageBroadcastMockInserter(
      content::WebContents* web_contents,
      testing::NiceMock<MockPageBroadcast>* mock_page_broadcast)
      : WebContentsObserver(web_contents),
        mock_page_broadcast_(mock_page_broadcast) {}

  void RenderViewHostChanged(RenderViewHost* old_host,
                             RenderViewHost* new_host) override {
    static_cast<TestRenderViewHost*>(new_host)->BindPageBroadcast(
        mock_page_broadcast_->GetRemote());
  }

 private:
  raw_ptr<testing::NiceMock<MockPageBroadcast>> mock_page_broadcast_;
};

// Test that navigations across browsing context groups trigger a page broadcast
// with up to date browsing context group information.
TEST_F(NavigationControllerTest, BrowsingContextGroupUpdate) {
  const GURL url1("http://a/");
  const GURL url2("chrome://ukm");

  // Start on a first page.
  NavigateAndCommit(url1);
  SiteInstanceImpl* initial_instance = main_test_rfh()->GetSiteInstance();

  // Setup the page broadcast expectations. We expect no call to be made, as the
  // RenderViewHost for B will get its update through the local frame commit.
  testing::NiceMock<MockPageBroadcast> mock_page_broadcast;
  EXPECT_CALL(mock_page_broadcast, UpdatePageBrowsingContextGroup(testing::_))
      .Times(0);
  PageBroadcastMockInserter mock_inserter(contents(), &mock_page_broadcast);

  // Navigate to a cross browsing context group page. The update function should
  // not be called.
  NavigateAndCommit(url2);
  SiteInstanceImpl* final_instance = main_test_rfh()->GetSiteInstance();
  EXPECT_FALSE(initial_instance->IsRelatedSiteInstance(final_instance));
}

class NavigationControllerFencedFrameTest : public NavigationControllerTest {
 public:
  NavigationControllerFencedFrameTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        blink::features::kFencedFrames, {{"implementation_type", "mparch"}});
  }
  ~NavigationControllerFencedFrameTest() override = default;

 protected:
  static constexpr char kTestRewriteURL[] = "http://test.com";
  static constexpr char kRewrittenURL[] = "http://rewritten.com";

  static bool URLRewriter(GURL* url, BrowserContext* browser_context) {
    if (*url == GURL(kTestRewriteURL)) {
      *url = GURL(kRewrittenURL);
      return true;
    }
    return false;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that receiving a request to navigate a fenced frame will not rewrite
// the fenced frame URL.
TEST_F(NavigationControllerFencedFrameTest, NoURLRewriteForFencedFrames) {
  const GURL kUrl1("http://google.com");
  const GURL kUrl2("http://chromium.org");

  // First, set up a handler that will rewrite urls.
  BrowserURLHandlerImpl::GetInstance()->AddHandlerPair(
      &URLRewriter, BrowserURLHandler::null_handler());

  // Simulate navigating to a page that has a fenced frame.
  NavigationSimulator::NavigateAndCommitFromDocument(kUrl1, main_test_rfh());
  RenderFrameHostImpl* fenced_frame_root = main_test_rfh()->AppendFencedFrame();
  // Navigate fenced frame.
  std::unique_ptr<NavigationSimulator> navigation_simulator =
      NavigationSimulator::CreateRendererInitiated(kUrl2, fenced_frame_root);
  navigation_simulator->Commit();
  fenced_frame_root = static_cast<RenderFrameHostImpl*>(
      navigation_simulator->GetFinalRenderFrameHost());

  // Simulate the fenced frame receiving a request from a RenderFrameProxyHost
  // to navigate to `kTestRewriteURL`.
  FrameTree* fenced_frame_tree = fenced_frame_root->frame_tree();
  fenced_frame_tree->controller().NavigateFromFrameProxy(
      fenced_frame_root, GURL(kTestRewriteURL),
      nullptr /* initiator_frame_token */,
      ChildProcessHost::kInvalidUniqueID /* initiator_process_id */,
      url::Origin::Create(kUrl2), /* initiator_base_url= */ std::nullopt,
      true /* is_renderer_initiated */, fenced_frame_root->GetSiteInstance(),
      Referrer(), ui::PAGE_TRANSITION_LINK,
      false /* should_replace_current_entry */,
      blink::NavigationDownloadPolicy(), "GET", nullptr, "",
      network::mojom::SourceLocation::New(), nullptr,
      false /*is_form_submission*/, std::nullopt,
      blink::mojom::NavigationInitiatorActivationAndAdStatus::
          kDidNotStartWithTransientActivation,
      base::TimeTicks::Now() /* navigation_start_time */);

  NavigationRequest* request =
      fenced_frame_root->frame_tree_node()->navigation_request();
  ASSERT_TRUE(request);
  // Ensure that the URL is not rewritten.
  EXPECT_EQ(GURL(kTestRewriteURL), request->GetURL());

  // Clean up the handler.
  BrowserURLHandlerImpl::GetInstance()->RemoveHandlerForTesting(&URLRewriter);
}

}  // namespace content
