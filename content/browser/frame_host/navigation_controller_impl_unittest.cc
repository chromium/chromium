// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/navigation_controller_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <tuple>
#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/gtest_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/browser_url_handler_impl.h"
#include "content/browser/frame_host/frame_navigation_entry.h"
#include "content/browser/frame_host/navigation_entry_impl.h"
#include "content/browser/frame_host/navigation_entry_screenshot_manager.h"
#include "content/browser/frame_host/navigation_handle_impl.h"
#include "content/browser/frame_host/navigation_request.h"
#include "content/browser/frame_host/navigator.h"
#include "content/browser/frame_host/navigator_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/frame_messages.h"
#include "content/common/frame_owner_properties.h"
#include "content/common/view_messages.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/page_state.h"
#include "content/public/common/page_type.h"
#include "content/public/common/previews_state.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_side_navigation_test_utils.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_navigation_ui_data.h"
#include "content/public/test/test_utils.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "skia/ext/platform_canvas.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/frame/frame_policy.h"

using base::Time;

namespace {

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
  return memcmp(a_bitmap.getPixels(), b_bitmap.getPixels(),
                a_bitmap.computeByteSize()) == 0;
}

class MockScreenshotManager : public content::NavigationEntryScreenshotManager {
 public:
  explicit MockScreenshotManager(content::NavigationControllerImpl* owner)
      : content::NavigationEntryScreenshotManager(owner),
        encoding_screenshot_in_progress_(false) {
  }

  ~MockScreenshotManager() override {}

  void TakeScreenshotFor(content::NavigationEntryImpl* entry) {
    SkBitmap bitmap;
    bitmap.allocPixels(SkImageInfo::Make(
        1, 1, kAlpha_8_SkColorType, kPremul_SkAlphaType));
    bitmap.eraseARGB(0, 0, 0, 0);
    encoding_screenshot_in_progress_ = true;
    OnScreenshotTaken(entry->GetUniqueID(), bitmap);
    WaitUntilScreenshotIsReady();
  }

  int GetScreenshotCount() {
    return content::NavigationEntryScreenshotManager::GetScreenshotCount();
  }

  void WaitUntilScreenshotIsReady() {
    if (!encoding_screenshot_in_progress_)
      return;
    message_loop_runner_ = new content::MessageLoopRunner;
    message_loop_runner_->Run();
  }

 private:
  void OnScreenshotSet(content::NavigationEntryImpl* entry) override {
    encoding_screenshot_in_progress_ = false;
    NavigationEntryScreenshotManager::OnScreenshotSet(entry);
    if (message_loop_runner_.get())
      message_loop_runner_->Quit();
  }

  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;
  bool encoding_screenshot_in_progress_;

  DISALLOW_COPY_AND_ASSIGN(MockScreenshotManager);
};

int64_t GenerateSequenceNumber() {
  // Based on how Blink generates sequence numbers.
  static int64_t next_number = base::Time::Now().ToDoubleT() * 1000000;
  return ++next_number;
}

}  // namespace

namespace content {

// TimeSmoother tests ----------------------------------------------------------

// With no duplicates, GetSmoothedTime should be the identity
// function.
TEST(TimeSmoother, Basic) {
  NavigationControllerImpl::TimeSmoother smoother;
  for (int64_t i = 1; i < 1000; ++i) {
    base::Time t = base::Time::FromInternalValue(i);
    EXPECT_EQ(t, smoother.GetSmoothedTime(t));
  }
}

// With a single duplicate and timestamps thereafter increasing by one
// microsecond, the smoothed time should always be one behind.
TEST(TimeSmoother, SingleDuplicate) {
  NavigationControllerImpl::TimeSmoother smoother;
  base::Time t = base::Time::FromInternalValue(1);
  EXPECT_EQ(t, smoother.GetSmoothedTime(t));
  for (int64_t i = 1; i < 1000; ++i) {
    base::Time expected_t = base::Time::FromInternalValue(i + 1);
    t = base::Time::FromInternalValue(i);
    EXPECT_EQ(expected_t, smoother.GetSmoothedTime(t));
  }
}

// With k duplicates and timestamps thereafter increasing by one
// microsecond, the smoothed time should always be k behind.
TEST(TimeSmoother, ManyDuplicates) {
  const int64_t kNumDuplicates = 100;
  NavigationControllerImpl::TimeSmoother smoother;
  base::Time t = base::Time::FromInternalValue(1);
  for (int64_t i = 0; i < kNumDuplicates; ++i) {
    base::Time expected_t = base::Time::FromInternalValue(i + 1);
    EXPECT_EQ(expected_t, smoother.GetSmoothedTime(t));
  }
  for (int64_t i = 1; i < 1000; ++i) {
    base::Time expected_t =
        base::Time::FromInternalValue(i + kNumDuplicates);
    t = base::Time::FromInternalValue(i);
    EXPECT_EQ(expected_t, smoother.GetSmoothedTime(t));
  }
}

// If the clock jumps far back enough after a run of duplicates, it
// should immediately jump to that value.
TEST(TimeSmoother, ClockBackwardsJump) {
  const int64_t kNumDuplicates = 100;
  NavigationControllerImpl::TimeSmoother smoother;
  base::Time t = base::Time::FromInternalValue(1000);
  for (int64_t i = 0; i < kNumDuplicates; ++i) {
    base::Time expected_t = base::Time::FromInternalValue(i + 1000);
    EXPECT_EQ(expected_t, smoother.GetSmoothedTime(t));
  }
  t = base::Time::FromInternalValue(500);
  EXPECT_EQ(t, smoother.GetSmoothedTime(t));
}

// NavigationControllerTest ----------------------------------------------------

class NavigationControllerTest
    : public RenderViewHostImplTestHarness,
      public WebContentsObserver {
 public:
  NavigationControllerTest() {}

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    WebContents* web_contents = RenderViewHostImplTestHarness::web_contents();
    ASSERT_TRUE(web_contents);  // The WebContents should be created by now.
    WebContentsObserver::Observe(web_contents);
  }

  // WebContentsObserver:
  void DidStartNavigationToPendingEntry(const GURL& url,
                                        ReloadType reload_type) override {
    navigated_url_ = url;
    last_reload_type_ = reload_type;
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

  const GURL& navigated_url() const {
    return navigated_url_;
  }

  NavigationControllerImpl& controller_impl() {
    return static_cast<NavigationControllerImpl&>(controller());
  }

  bool HasNavigationRequest() {
    return contents()->GetFrameTree()->root()->navigation_request() != nullptr;
  }

  const GURL GetLastNavigationURL() {
    NavigationRequest* navigation_request =
        contents()->GetFrameTree()->root()->navigation_request();
    CHECK(navigation_request);
    return navigation_request->common_params().url;
  }

  content::PreviewsState GetLastNavigationPreviewsState() {
    NavigationRequest* navigation_request =
        contents()->GetFrameTree()->root()->navigation_request();
    CHECK(navigation_request);
    return navigation_request->common_params().previews_state;
  }

 protected:
  GURL navigated_url_;
  size_t navigation_entry_committed_counter_ = 0;
  size_t navigation_entry_changed_counter_ = 0;
  size_t navigation_list_pruned_counter_ = 0;
  size_t navigation_entries_deleted_counter_ = 0;
  PrunedDetails last_navigation_entry_pruned_details_;
  ReloadType last_reload_type_;
};

class TestWebContentsDelegate : public WebContentsDelegate {
 public:
  TestWebContentsDelegate()
      : navigation_state_change_count_(0), repost_form_warning_count_(0) {}

  int navigation_state_change_count() {
    return navigation_state_change_count_;
  }

  int repost_form_warning_count() {
    return repost_form_warning_count_;
  }

  // Keep track of whether the tab has notified us of a navigation state change.
  void NavigationStateChanged(WebContents* source,
                              InvalidateTypes changed_flags) override {
    navigation_state_change_count_++;
  }

  void ShowRepostFormWarningDialog(WebContents* source) override {
    repost_form_warning_count_++;
  }

 private:
  // The number of times NavigationStateChanged has been called.
  int navigation_state_change_count_;

  // The number of times ShowRepostFormWarningDialog() was called.
  int repost_form_warning_count_;
};

// Observer that records the LoadCommittedDetails from the most recent commit.
class LoadCommittedDetailsObserver : public WebContentsObserver {
 public:
  // Observes navigation for the specified |web_contents|.
  explicit LoadCommittedDetailsObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents),
        navigation_type_(NAVIGATION_TYPE_UNKNOWN),
        is_same_document_(false),
        is_main_frame_(false),
        did_replace_entry_(false) {}

  NavigationType navigation_type() { return navigation_type_; }
  const GURL& previous_url() { return previous_url_; }
  bool is_same_document() { return is_same_document_; }
  bool is_main_frame() { return is_main_frame_; }
  bool did_replace_entry() { return did_replace_entry_; }
  bool has_navigation_ui_data() { return has_navigation_ui_data_; }

 private:
  void DidFinishNavigation(NavigationHandle* navigation_handle) override {
    if (!navigation_handle->HasCommitted())
      return;

    navigation_type_ = static_cast<NavigationHandleImpl*>(navigation_handle)
                           ->navigation_type();
    previous_url_ = navigation_handle->GetPreviousURL();
    is_same_document_ = navigation_handle->IsSameDocument();
    is_main_frame_ = navigation_handle->IsInMainFrame();
    did_replace_entry_ = navigation_handle->DidReplaceEntry();
    has_navigation_ui_data_ = navigation_handle->GetNavigationUIData();
  }

  NavigationType navigation_type_;
  GURL previous_url_;
  bool is_same_document_;
  bool is_main_frame_;
  bool did_replace_entry_;
  bool has_navigation_ui_data_;
};

// PlzNavigate
// A NavigationControllerTest run with --enable-browser-side-navigation.
// TODO(clamy): Make those regular NavigationControllerTests.
class NavigationControllerTestWithBrowserSideNavigation
    : public NavigationControllerTest {
 public:
  void SetUp() override {
    NavigationControllerTest::SetUp();
  }
};

// -----------------------------------------------------------------------------

TEST_F(NavigationControllerTest, Defaults) {
  NavigationControllerImpl& controller = controller_impl();

  EXPECT_FALSE(controller.GetPendingEntry());
  EXPECT_FALSE(controller.GetVisibleEntry());
  EXPECT_FALSE(controller.GetLastCommittedEntry());
  EXPECT_EQ(controller.GetPendingEntryIndex(), -1);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), -1);
  EXPECT_EQ(controller.GetEntryCount(), 0);
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

  main_test_rfh()->SendRendererInitiatedNavigationRequest(urls[0], true);
  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigate(0, true, urls[0]);
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;
  EXPECT_EQ(urls[0], controller.GetVisibleEntry()->GetVirtualURL());
  EXPECT_FALSE(controller.CanGoBack());
  EXPECT_FALSE(controller.CanGoForward());
  EXPECT_FALSE(controller.CanGoToOffset(1));

  for (int i = 1; i <= 4; ++i) {
    main_test_rfh()->SendRendererInitiatedNavigationRequest(urls[i], true);
    main_test_rfh()->PrepareForCommit();
    main_test_rfh()->SendNavigate(0, true, urls[i]);
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

  const int test_offsets[NUM_TESTS] = {
    GO_TO_MIDDLE_PAGE,
    GO_FORWARDS,
    GO_BACKWARDS,
    GO_TO_BEGINNING,
    GO_TO_END
  };

  for (int test = 0; test < NUM_TESTS; ++test) {
    int offset = test_offsets[test];
    controller.GoToOffset(offset);
    int entry_id = controller.GetPendingEntry()->GetUniqueID();
    url_index += offset;
    // Check that the GoToOffset will land on the expected page.
    EXPECT_EQ(urls[url_index], controller.GetPendingEntry()->GetVirtualURL());
    main_test_rfh()->PrepareForCommit();
    main_test_rfh()->SendNavigateWithTransition(
        entry_id, false, urls[url_index],
        controller.GetPendingEntry()->GetTransitionType());
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

// Tests that receiving a DidFailProvisionalLoad from the renderer that is
// trying to commit an error page won't reset the pending entry of a navigation
// that just started.
TEST_F(NavigationControllerTestWithBrowserSideNavigation,
       DontDiscardWrongPendingEntry) {
  NavigationControllerImpl& controller = controller_impl();
  GURL initial_url("http://www.google.com");
  GURL url_1("http://google.com/foo");
  GURL url_2("http://foo2.com");

  // Navigate inititally. This is the url that could erroneously be the visible
  // entry when url_1 fails.
  NavigateAndCommit(initial_url);

  // Set the pending entry as url_1 and create the NavigationHandle.
  controller.LoadURL(url_1, Referrer(), ui::PAGE_TRANSITION_TYPED,
                     std::string());
  EXPECT_EQ(url_1, controller.GetVisibleEntry()->GetURL());

  // The navigation fails and needs to show an error page. This resets the
  // pending entry.
  main_test_rfh()->SimulateNavigationError(url_1, net::ERR_TIMED_OUT);
  EXPECT_EQ(initial_url, controller.GetVisibleEntry()->GetURL());

  // A navigation to url_2 starts, creating a pending navigation entry.
  controller.LoadURL(url_2, Referrer(), ui::PAGE_TRANSITION_TYPED,
                     std::string());
  EXPECT_EQ(url_2, controller.GetVisibleEntry()->GetURL());

  // The DidFailProvsionalLoad IPC is received from the current RFH that is
  // committing an error page. This should not reset the pending entry for the
  // new ongoing navigation.
  FrameHostMsg_DidFailProvisionalLoadWithError_Params error;
  error.error_code = net::ERR_TIMED_OUT;
  error.url = url_1;
  main_test_rfh()->OnMessageReceived(
      FrameHostMsg_DidFailProvisionalLoadWithError(
          main_test_rfh()->GetRoutingID(), error));
  EXPECT_EQ(url_2, controller.GetVisibleEntry()->GetURL());
}

TEST_F(NavigationControllerTest, LoadURL) {
  NavigationControllerImpl& controller = controller_impl();

  const GURL url1("http://foo1");
  const GURL url2("http://foo2");

  controller.LoadURL(
      url1, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  int entry_id = controller.GetPendingEntry()->GetUniqueID();
  // Creating a pending notification should not have issued any of the
  // notifications we're listening for.
  EXPECT_EQ(0U, navigation_entry_changed_counter_);
  EXPECT_EQ(0U, navigation_list_pruned_counter_);

  // The load should now be pending.
  EXPECT_EQ(controller.GetEntryCount(), 0);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), -1);
  EXPECT_EQ(controller.GetPendingEntryIndex(), -1);
  EXPECT_FALSE(controller.GetLastCommittedEntry());
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

  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigate(entry_id, true, url1);
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;

  // The load should now be committed.
  EXPECT_EQ(controller.GetEntryCount(), 1);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), 0);
  EXPECT_EQ(controller.GetPendingEntryIndex(), -1);
  EXPECT_TRUE(controller.GetLastCommittedEntry());
  EXPECT_FALSE(controller.GetPendingEntry());
  ASSERT_TRUE(controller.GetVisibleEntry());
  EXPECT_FALSE(controller.CanGoBack());
  EXPECT_FALSE(controller.CanGoForward());
  EXPECT_EQ(0, controller.GetLastCommittedEntry()->bindings());

  // The timestamp should have been set.
  EXPECT_FALSE(controller.GetVisibleEntry()->GetTimestamp().is_null());

  // Load another...
  controller.LoadURL(
      url2, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  entry_id = controller.GetPendingEntry()->GetUniqueID();

  // The load should now be pending.
  EXPECT_EQ(controller.GetEntryCount(), 1);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), 0);
  EXPECT_EQ(controller.GetPendingEntryIndex(), -1);
  EXPECT_TRUE(controller.GetLastCommittedEntry());
  ASSERT_TRUE(controller.GetPendingEntry());
  EXPECT_EQ(controller.GetPendingEntry(), controller.GetVisibleEntry());
  // TODO(darin): maybe this should really be true?
  EXPECT_FALSE(controller.CanGoBack());
  EXPECT_FALSE(controller.CanGoForward());

  EXPECT_TRUE(controller.GetPendingEntry()->GetTimestamp().is_null());

  // Simulate the beforeunload ack for the cross-site transition, and then the
  // commit.
  main_test_rfh()->PrepareForCommit();
  contents()->GetPendingMainFrame()->SendNavigate(entry_id, true, url2);
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;

  // The load should now be committed.
  EXPECT_EQ(controller.GetEntryCount(), 2);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), 1);
  EXPECT_EQ(controller.GetPendingEntryIndex(), -1);
  EXPECT_TRUE(controller.GetLastCommittedEntry());
  EXPECT_FALSE(controller.GetPendingEntry());
  ASSERT_TRUE(controller.GetVisibleEntry());
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
      base::Bind(&GetFixedTime, base::Time::FromInternalValue(1)));

  const GURL url1("http://foo1");
  const GURL url2("http://foo2");

  controller.LoadURL(
      url1, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  int entry_id = controller.GetPendingEntry()->GetUniqueID();

  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigate(entry_id, true, url1);
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;

  // Load another...
  controller.LoadURL(
      url2, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  entry_id = controller.GetPendingEntry()->GetUniqueID();

  // Simulate the beforeunload ack for the cross-site transition, and then the
  // commit.
  main_test_rfh()->PrepareForCommit();
  contents()->GetPendingMainFrame()->SendNavigate(entry_id, true, url2);
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;

  // The two loads should now be committed.
  ASSERT_EQ(controller.GetEntryCount(), 2);

  // Timestamps should be distinct despite the clock returning the
  // same value.
  EXPECT_EQ(1u,
            controller.GetEntryAtIndex(0)->GetTimestamp().ToInternalValue());
  EXPECT_EQ(2u,
            controller.GetEntryAtIndex(1)->GetTimestamp().ToInternalValue());
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
  if (!load_params.virtual_url_for_data_url.is_empty()) {
    EXPECT_EQ(load_params.virtual_url_for_data_url, entry->GetVirtualURL());
  }
#if defined(OS_ANDROID)
  EXPECT_EQ(load_params.data_url_as_string, entry->GetDataURLAsString());
#endif
  if (NavigationController::UA_OVERRIDE_INHERIT !=
      load_params.override_user_agent) {
    bool should_override = (NavigationController::UA_OVERRIDE_TRUE ==
        load_params.override_user_agent);
    EXPECT_EQ(should_override, entry->GetIsOverridingUserAgent());
  }
  EXPECT_EQ(load_params.post_data, entry->GetPostData());
}

TEST_F(NavigationControllerTest, LoadURLWithParams) {
  // Start a navigation in order to have enough state to fake a transfer.
  const GURL url1("http://foo");
  const GURL url2("http://bar");

  contents()->NavigateAndCommit(url1);
  auto navigation =
      NavigationSimulator::CreateBrowserInitiated(url2, contents());
  navigation->Start();

  NavigationControllerImpl& controller = controller_impl();

  NavigationController::LoadURLParams load_params(GURL("http://foo/2"));
  load_params.referrer = Referrer(GURL("http://referrer"),
                                  network::mojom::ReferrerPolicy::kDefault);
  load_params.transition_type = ui::PAGE_TRANSITION_GENERATED;
  load_params.extra_headers = "content-type: text/plain;\nX-Foo: Bar";
  load_params.load_type = NavigationController::LOAD_TYPE_DEFAULT;
  load_params.is_renderer_initiated = true;
  load_params.override_user_agent = NavigationController::UA_OVERRIDE_TRUE;

  controller.LoadURLWithParams(load_params);
  NavigationEntryImpl* entry = controller.GetPendingEntry();

  // The timestamp should not have been set yet.
  ASSERT_TRUE(entry);
  EXPECT_TRUE(entry->GetTimestamp().is_null());

  CheckNavigationEntryMatchLoadParams(load_params, entry);
}

TEST_F(NavigationControllerTest, LoadURLWithExtraParams_Data) {
  NavigationControllerImpl& controller = controller_impl();

  NavigationController::LoadURLParams load_params(
      GURL("data:text/html,dataurl"));
  load_params.load_type = NavigationController::LOAD_TYPE_DATA;
  load_params.base_url_for_data_url = GURL("http://foo");
  load_params.virtual_url_for_data_url = GURL(url::kAboutBlankURL);
  load_params.override_user_agent = NavigationController::UA_OVERRIDE_FALSE;

  controller.LoadURLWithParams(load_params);
  NavigationEntryImpl* entry = controller.GetPendingEntry();

  CheckNavigationEntryMatchLoadParams(load_params, entry);
}

#if defined(OS_ANDROID)
TEST_F(NavigationControllerTest, LoadURLWithExtraParams_Data_Android) {
  NavigationControllerImpl& controller = controller_impl();

  NavigationController::LoadURLParams load_params(GURL("data:,"));
  load_params.load_type = NavigationController::LOAD_TYPE_DATA;
  load_params.base_url_for_data_url = GURL("http://foo");
  load_params.virtual_url_for_data_url = GURL(url::kAboutBlankURL);
  std::string s("data:,data");
  load_params.data_url_as_string = base::RefCountedString::TakeString(&s);
  load_params.override_user_agent = NavigationController::UA_OVERRIDE_FALSE;

  controller.LoadURLWithParams(load_params);
  NavigationEntryImpl* entry = controller.GetPendingEntry();

  CheckNavigationEntryMatchLoadParams(load_params, entry);
}
#endif

TEST_F(NavigationControllerTest, LoadURLWithExtraParams_HttpPost) {
  NavigationControllerImpl& controller = controller_impl();

  NavigationController::LoadURLParams load_params(GURL("https://posturl"));
  load_params.transition_type = ui::PAGE_TRANSITION_TYPED;
  load_params.load_type = NavigationController::LOAD_TYPE_HTTP_POST;
  load_params.override_user_agent = NavigationController::UA_OVERRIDE_TRUE;

  const char* raw_data = "d\n\0a2";
  const int length = 5;
  load_params.post_data =
      network::ResourceRequestBody::CreateFromBytes(raw_data, length);

  controller.LoadURLWithParams(load_params);
  NavigationEntryImpl* entry = controller.GetPendingEntry();

  CheckNavigationEntryMatchLoadParams(load_params, entry);
}

// Tests what happens when the same page is loaded again.  Should not create a
// new session history entry. This is what happens when you press enter in the
// URL bar to reload: a pending entry is created and then it is discarded when
// the load commits (because WebCore didn't actually make a new entry).
TEST_F(NavigationControllerTest, LoadURL_SamePage) {
  NavigationControllerImpl& controller = controller_impl();

  const GURL url1("http://foo1");

  controller.LoadURL(
      url1, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  int entry_id = controller.GetPendingEntry()->GetUniqueID();
  EXPECT_EQ(0U, navigation_entry_changed_counter_);
  EXPECT_EQ(0U, navigation_list_pruned_counter_);
  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigateWithTransition(
      entry_id, true, url1, ui::PAGE_TRANSITION_TYPED);
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;

  ASSERT_TRUE(controller.GetVisibleEntry());
  const base::Time timestamp = controller.GetVisibleEntry()->GetTimestamp();
  EXPECT_FALSE(timestamp.is_null());

  const std::string new_extra_headers("Foo: Bar\nBar: Baz");
  controller.LoadURL(
      url1, Referrer(), ui::PAGE_TRANSITION_TYPED, new_extra_headers);
  entry_id = controller.GetPendingEntry()->GetUniqueID();
  EXPECT_EQ(0U, navigation_entry_changed_counter_);
  EXPECT_EQ(0U, navigation_list_pruned_counter_);
  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigateWithTransition(
      entry_id, false, url1, ui::PAGE_TRANSITION_TYPED);
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

// Load the same page twice, once as a GET and once as a POST.
// We should update the post state on the NavigationEntry.
TEST_F(NavigationControllerTest, LoadURL_SamePage_DifferentMethod) {
  NavigationControllerImpl& controller = controller_impl();

  const GURL url1("http://foo1");

  controller.LoadURL(
      url1, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  FrameHostMsg_DidCommitProvisionalLoad_Params params;
  params.nav_entry_id = controller.GetPendingEntry()->GetUniqueID();
  params.did_create_new_entry = true;
  params.url = url1;
  params.transition = ui::PAGE_TRANSITION_TYPED;
  params.method = "POST";
  params.post_id = 123;
  params.page_state =
      PageState::CreateForTesting(url1, false, nullptr, nullptr);
  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigateWithParams(&params, false);

  // The post data should be visible.
  NavigationEntry* entry = controller.GetVisibleEntry();
  ASSERT_TRUE(entry);
  EXPECT_TRUE(entry->GetHasPostData());
  EXPECT_EQ(entry->GetPostID(), 123);

  controller.LoadURL(
      url1, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigateWithTransition(
      controller.GetPendingEntry()->GetUniqueID(),
      false, url1, ui::PAGE_TRANSITION_TYPED);

  // We should not have produced a new session history entry.
  ASSERT_EQ(controller.GetVisibleEntry(), entry);

  // The post data should have been cleared due to the GET.
  EXPECT_FALSE(entry->GetHasPostData());
  EXPECT_EQ(entry->GetPostID(), 0);
}

// Tests loading a URL but discarding it before the load commits.
TEST_F(NavigationControllerTest, LoadURL_Discarded) {
  NavigationControllerImpl& controller = controller_impl();

  const GURL url1("http://foo1");
  const GURL url2("http://foo2");

  controller.LoadURL(
      url1, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  int entry_id = controller.GetPendingEntry()->GetUniqueID();
  EXPECT_EQ(0U, navigation_entry_changed_counter_);
  EXPECT_EQ(0U, navigation_list_pruned_counter_);
  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigate(entry_id, true, url1);
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;

  ASSERT_TRUE(controller.GetVisibleEntry());
  const base::Time timestamp = controller.GetVisibleEntry()->GetTimestamp();
  EXPECT_FALSE(timestamp.is_null());

  controller.LoadURL(
      url2, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
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
  controller.LoadURL(
      kExistingURL1, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  int entry_id = controller.GetPendingEntry()->GetUniqueID();
  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigate(entry_id, true, kExistingURL1);
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
  controller.LoadURL(
      kExistingURL1, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  int entry_id = controller.GetPendingEntry()->GetUniqueID();
  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigate(entry_id, true, kExistingURL1);
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;

  // Make a pending entry to somewhere new.
  const GURL kExistingURL2("http://bee");
  controller.LoadURL(
      kExistingURL2, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  EXPECT_EQ(0U, navigation_entry_changed_counter_);
  EXPECT_EQ(0U, navigation_list_pruned_counter_);

  // After the beforeunload but before it commits...
  main_test_rfh()->PrepareForCommit();

  // ... Do a new navigation.
  const GURL kNewURL("http://see");
  main_test_rfh()->SendRendererInitiatedNavigationRequest(kNewURL, true);
  main_test_rfh()->PrepareForCommit();
  contents()->GetMainFrame()->SendNavigate(0, true, kNewURL);

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
  controller.LoadURL(
      kExistingURL1, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  int entry_id = controller.GetPendingEntry()->GetUniqueID();
  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigate(entry_id, true, kExistingURL1);
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;

  const GURL kExistingURL2("http://foo/bee");
  controller.LoadURL(
      kExistingURL2, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  entry_id = controller.GetPendingEntry()->GetUniqueID();
  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigate(entry_id, true, kExistingURL2);
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
  main_test_rfh()->SendRendererInitiatedNavigationRequest(kNewURL, true);
  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigate(0, true, kNewURL);

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
  const GURL kExistingURL1("http://privileged");
  controller.LoadURL(
      kExistingURL1, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  int entry_id = controller.GetPendingEntry()->GetUniqueID();
  // Pretend it has bindings so we can tell if we incorrectly copy it.
  main_test_rfh()->AllowBindings(BINDINGS_POLICY_MOJO_WEB_UI);
  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigate(entry_id, true, kExistingURL1);
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;

  // Navigate cross-process to a second URL.
  const GURL kExistingURL2("http://foo/eh");
  controller.LoadURL(
      kExistingURL2, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  entry_id = controller.GetPendingEntry()->GetUniqueID();
  main_test_rfh()->PrepareForCommit();
  TestRenderFrameHost* foo_rfh = contents()->GetPendingMainFrame();
  foo_rfh->SendNavigate(entry_id, true, kExistingURL2);
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;

  // Now make a pending back/forward navigation to a privileged entry.
  // The zeroth entry should be pending.
  controller.GoBack();
  foo_rfh->SendBeforeUnloadACK(true);
  EXPECT_EQ(0U, navigation_entry_changed_counter_);
  EXPECT_EQ(0U, navigation_list_pruned_counter_);
  EXPECT_EQ(0, controller.GetPendingEntryIndex());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(2, controller.GetPendingEntry()->bindings());

  // Before that commits, do a new navigation.
  const GURL kNewURL("http://foo/bee");
  foo_rfh->SendRendererInitiatedNavigationRequest(kNewURL, true);
  foo_rfh->PrepareForCommit();
  foo_rfh->SendNavigate(0, true, kNewURL);

  // There should no longer be any pending entry, and the new navigation we
  // just made should be committed.
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;
  EXPECT_EQ(-1, controller.GetPendingEntryIndex());
  EXPECT_EQ(2, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(kNewURL, controller.GetVisibleEntry()->GetURL());
  EXPECT_EQ(0, controller.GetLastCommittedEntry()->bindings());
}

// Tests navigating to an existing URL when there is a pending new navigation.
// This will happen if the user enters a URL, but before that commits, the
// current page fires history.back().
TEST_F(NavigationControllerTest, LoadURL_BackPreemptsPending) {
  NavigationControllerImpl& controller = controller_impl();

  // First make some history.
  const GURL kExistingURL1("http://foo/eh");
  controller.LoadURL(
      kExistingURL1, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  int entry_id = controller.GetPendingEntry()->GetUniqueID();
  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigate(entry_id, true, kExistingURL1);
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;

  const GURL kExistingURL2("http://foo/bee");
  controller.LoadURL(
      kExistingURL2, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  entry_id = controller.GetPendingEntry()->GetUniqueID();
  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigate(entry_id, true, kExistingURL2);
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;

  // A back navigation comes in from the renderer...
  controller.GoToOffset(-1);
  entry_id = controller.GetPendingEntry()->GetUniqueID();

  // ...while the user tries to navigate to a new page...
  const GURL kNewURL("http://foo/see");
  controller.LoadURL(
      kNewURL, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  EXPECT_EQ(0U, navigation_entry_changed_counter_);
  EXPECT_EQ(0U, navigation_list_pruned_counter_);
  EXPECT_EQ(-1, controller.GetPendingEntryIndex());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());

  // ...and the back navigation commits.
  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigate(entry_id, false, kExistingURL1);

  // There should no longer be any pending entry, and the back navigation should
  // be committed.
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;
  EXPECT_EQ(-1, controller.GetPendingEntryIndex());
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(kExistingURL1, controller.GetVisibleEntry()->GetURL());
}

// Tests an ignored navigation when there is a pending new navigation.
// This will happen if the user enters a URL, but before that commits, the
// current blank page reloads.  See http://crbug.com/77507.
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
  controller.LoadURL(
      kNewURL, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  EXPECT_EQ(0U, navigation_entry_changed_counter_);
  EXPECT_EQ(0U, navigation_list_pruned_counter_);
  EXPECT_EQ(-1, controller.GetPendingEntryIndex());
  EXPECT_TRUE(controller.GetPendingEntry());
  EXPECT_EQ(-1, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(1, delegate->navigation_state_change_count());

  // Before that commits, a document.write and location.reload can cause the
  // renderer to send a FrameNavigate with nav_entry_id 0.
  // PlzNavigate: this will stop the old navigation and start a new one.
  main_test_rfh()->SendRendererInitiatedNavigationRequest(kExistingURL, true);
  main_test_rfh()->SendNavigate(0, false, kExistingURL);

  // This should clear the pending entry and notify of a navigation state
  // change, so that we do not keep displaying kNewURL.
  EXPECT_EQ(-1, controller.GetPendingEntryIndex());
  EXPECT_FALSE(controller.GetPendingEntry());
  EXPECT_EQ(-1, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(2, delegate->navigation_state_change_count());

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
  controller.LoadURL(
      kNewURL, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  main_test_rfh()->PrepareForCommit();
  EXPECT_EQ(0U, navigation_entry_changed_counter_);
  EXPECT_EQ(0U, navigation_list_pruned_counter_);
  EXPECT_EQ(-1, controller.GetPendingEntryIndex());
  EXPECT_TRUE(controller.GetPendingEntry());
  EXPECT_EQ(-1, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(1, delegate->navigation_state_change_count());

  // It may abort before committing, if it's a download or due to a stop or
  // a new navigation from the user.
  FrameHostMsg_DidFailProvisionalLoadWithError_Params params;
  params.error_code = net::ERR_ABORTED;
  params.error_description = base::string16();
  params.url = kNewURL;
  params.showing_repost_interstitial = false;
  main_test_rfh()->OnMessageReceived(
      FrameHostMsg_DidFailProvisionalLoadWithError(0,  // routing_id
                                                   params));

  // This should not clear the pending entry or notify of a navigation state
  // change, so that we keep displaying kNewURL (until the user clears it).
  EXPECT_EQ(-1, controller.GetPendingEntryIndex());
  EXPECT_TRUE(controller.GetPendingEntry());
  EXPECT_EQ(-1, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(1, delegate->navigation_state_change_count());
  NavigationEntry* pending_entry = controller.GetPendingEntry();

  // Ensure that a reload keeps the same pending entry.
  controller.Reload(ReloadType::NORMAL, true);
  EXPECT_EQ(-1, controller.GetPendingEntryIndex());
  EXPECT_TRUE(controller.GetPendingEntry());
  EXPECT_EQ(pending_entry, controller.GetPendingEntry());
  EXPECT_EQ(-1, controller.GetLastCommittedEntryIndex());

  contents()->SetDelegate(nullptr);
}

// Tests that the pending URL is not visible during a renderer-initiated
// redirect and abort.  See http://crbug.com/83031.
TEST_F(NavigationControllerTest, LoadURL_RedirectAbortDoesntShowPendingURL) {
  NavigationControllerImpl& controller = controller_impl();

  // First make an existing committed entry.
  const GURL kExistingURL("http://foo/eh");
  controller.LoadURL(kExistingURL, content::Referrer(),
                     ui::PAGE_TRANSITION_TYPED, std::string());
  int entry_id = controller.GetPendingEntry()->GetUniqueID();
  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigate(entry_id, true, kExistingURL);
  main_test_rfh()->OnMessageReceived(FrameHostMsg_DidStopLoading(0));
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

// Ensure that NavigationEntries track which bindings their RenderViewHost had
// at the time they committed.  http://crbug.com/173672.
TEST_F(NavigationControllerTest, LoadURL_WithBindings) {
  NavigationControllerImpl& controller = controller_impl();
  std::vector<GURL> url_chain;

  const GURL url1("http://foo1");
  const GURL url2("http://foo2");

  // Navigate to a first, unprivileged URL.
  controller.LoadURL(
      url1, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  EXPECT_EQ(NavigationEntryImpl::kInvalidBindings,
            controller.GetPendingEntry()->bindings());
  int entry1_id = controller.GetPendingEntry()->GetUniqueID();

  // Commit.
  TestRenderFrameHost* orig_rfh = contents()->GetMainFrame();
  orig_rfh->PrepareForCommit();
  orig_rfh->SendNavigate(entry1_id, true, url1);
  EXPECT_EQ(controller.GetEntryCount(), 1);
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(0, controller.GetLastCommittedEntry()->bindings());
  entry1_id = controller.GetLastCommittedEntry()->GetUniqueID();

  // Manually increase the number of active frames in the SiteInstance
  // that orig_rfh belongs to, to prevent it from being destroyed when
  // it gets swapped out, so that we can reuse orig_rfh when the
  // controller goes back.
  orig_rfh->GetSiteInstance()->IncrementActiveFrameCount();

  // Navigate to a second URL, simulate the beforeunload ack for the cross-site
  // transition, and set bindings on the pending RenderViewHost to simulate a
  // privileged url.
  controller.LoadURL(
      url2, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  int entry_id = controller.GetPendingEntry()->GetUniqueID();
  orig_rfh->PrepareForCommit();
  TestRenderFrameHost* new_rfh = contents()->GetPendingMainFrame();
  new_rfh->AllowBindings(BINDINGS_POLICY_WEB_UI);
  new_rfh->SendNavigate(entry_id, true, url2);

  // The second load should be committed, and bindings should be remembered.
  EXPECT_EQ(controller.GetEntryCount(), 2);
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  EXPECT_TRUE(controller.CanGoBack());
  EXPECT_EQ(1, controller.GetLastCommittedEntry()->bindings());

  // Going back, the first entry should still appear unprivileged.
  controller.GoBack();
  new_rfh->PrepareForCommit();
  contents()->GetPendingMainFrame()->SendNavigateWithTransition(
      entry1_id, false, url1,
      controller.GetPendingEntry()->GetTransitionType());
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(0, controller.GetLastCommittedEntry()->bindings());
}

TEST_F(NavigationControllerTest, Reload) {
  NavigationControllerImpl& controller = controller_impl();

  const GURL url1("http://foo1");

  controller.LoadURL(
      url1, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  int entry_id = controller.GetPendingEntry()->GetUniqueID();
  EXPECT_EQ(0U, navigation_entry_changed_counter_);
  EXPECT_EQ(0U, navigation_list_pruned_counter_);
  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigate(entry_id, true, url1);
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;
  ASSERT_TRUE(controller.GetVisibleEntry());
  entry_id = controller.GetLastCommittedEntry()->GetUniqueID();

  controller.Reload(ReloadType::NORMAL, true);
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

  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigate(entry_id, false, url1);
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

  // The timestamp should have been updated.
  ASSERT_TRUE(controller.GetVisibleEntry());
  EXPECT_GE(controller.GetVisibleEntry()->GetTimestamp(), timestamp);
}

// Tests what happens when a reload navigation produces a new page.
TEST_F(NavigationControllerTest, Reload_GeneratesNewPage) {
  NavigationControllerImpl& controller = controller_impl();

  const GURL url1("http://foo1");
  const GURL url2("http://foo2");

  controller.LoadURL(
      url1, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  int entry_id = controller.GetPendingEntry()->GetUniqueID();
  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigate(entry_id, true, url1);
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;
  entry_id = controller.GetLastCommittedEntry()->GetUniqueID();

  controller.Reload(ReloadType::NORMAL, true);
  EXPECT_EQ(0U, navigation_entry_changed_counter_);
  EXPECT_EQ(0U, navigation_list_pruned_counter_);

  main_test_rfh()->PrepareForCommitWithServerRedirect(url2);
  main_test_rfh()->SendNavigate(entry_id, true, url2);
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;

  // Now the reload is committed.
  EXPECT_EQ(controller.GetEntryCount(), 2);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), 1);
  EXPECT_EQ(controller.GetPendingEntryIndex(), -1);
  EXPECT_TRUE(controller.GetLastCommittedEntry());
  EXPECT_FALSE(controller.GetPendingEntry());
  EXPECT_TRUE(controller.CanGoBack());
  EXPECT_FALSE(controller.CanGoForward());
}

// This test ensures that when a guest renderer reloads, the reload goes through
// without ending up in the "we have a wrong process for the URL" branch in
// NavigationControllerImpl::ReloadInternal.
TEST_F(NavigationControllerTest, ReloadWithGuest) {
  NavigationControllerImpl& controller = controller_impl();

  const GURL url1("http://foo1");
  controller.LoadURL(
      url1, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  int entry_id = controller.GetPendingEntry()->GetUniqueID();
  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigate(entry_id, true, url1);
  ASSERT_TRUE(controller.GetVisibleEntry());

  // Make the entry believe its RenderProcessHost is a guest.
  NavigationEntryImpl* entry1 = controller.GetVisibleEntry();
  reinterpret_cast<MockRenderProcessHost*>(
      entry1->site_instance()->GetProcess())->set_is_for_guests_only(true);

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

namespace {
void SetOriginalURL(const GURL& url,
                    FrameHostMsg_DidCommitProvisionalLoad_Params* params) {
  params->original_request_url = url;
}
}

TEST_F(NavigationControllerTest, ReloadOriginalRequestURL) {
  NavigationControllerImpl& controller = controller_impl();

  const GURL original_url("http://foo1");
  const GURL final_url("http://foo2");
  auto set_original_url_callback = base::Bind(SetOriginalURL, original_url);

  // Load up the original URL, but get redirected.
  controller.LoadURL(
      original_url, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  int entry_id = controller.GetPendingEntry()->GetUniqueID();
  EXPECT_EQ(0U, navigation_entry_changed_counter_);
  EXPECT_EQ(0U, navigation_list_pruned_counter_);
  main_test_rfh()->PrepareForCommitWithServerRedirect(final_url);
  main_test_rfh()->SendNavigateWithModificationCallback(
      entry_id, true, final_url, std::move(set_original_url_callback));
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;
  entry_id = controller.GetLastCommittedEntry()->GetUniqueID();

  // The NavigationEntry should save both the original URL and the final
  // redirected URL.
  EXPECT_EQ(
      original_url, controller.GetVisibleEntry()->GetOriginalRequestURL());
  EXPECT_EQ(final_url, controller.GetVisibleEntry()->GetURL());

  // Reload using the original URL.
  controller.Reload(ReloadType::ORIGINAL_REQUEST_URL, false);
  EXPECT_EQ(0U, navigation_entry_changed_counter_);
  EXPECT_EQ(0U, navigation_list_pruned_counter_);

  // The reload is pending.  The request should point to the original URL.
  EXPECT_EQ(original_url, navigated_url());
  EXPECT_EQ(controller.GetEntryCount(), 1);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), 0);
  EXPECT_EQ(controller.GetPendingEntryIndex(), 0);
  EXPECT_TRUE(controller.GetLastCommittedEntry());
  EXPECT_TRUE(controller.GetPendingEntry());
  EXPECT_FALSE(controller.CanGoBack());
  EXPECT_FALSE(controller.CanGoForward());

  // Send that the navigation has proceeded; say it got redirected again.
  main_test_rfh()->PrepareForCommitWithServerRedirect(final_url);
  main_test_rfh()->SendNavigate(entry_id, false, final_url);
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

// Test that certain non-persisted NavigationEntryImpl values get reset after
// commit.
TEST_F(NavigationControllerTest, ResetEntryValuesAfterCommit) {
  NavigationControllerImpl& controller = controller_impl();

  // The value of "should replace entry" will be tested, but it's an error to
  // specify it when there are no entries. Create a simple entry to be replaced.
  const GURL url0("http://foo/0");
  controller.LoadURL(
      url0, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  int entry_id = controller.GetPendingEntry()->GetUniqueID();
  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigate(entry_id, true, url0);

  // Set up the pending entry.
  const GURL url1("http://foo/1");
  controller.LoadURL(
      url1, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  entry_id = controller.GetPendingEntry()->GetUniqueID();

  // Set up some sample values.
  const char* raw_data = "post\n\n\0data";
  const int length = 11;

  // Set non-persisted values on the pending entry.
  NavigationEntryImpl* pending_entry = controller.GetPendingEntry();
  pending_entry->SetPostData(
      network::ResourceRequestBody::CreateFromBytes(raw_data, length));
  pending_entry->set_is_renderer_initiated(true);
  pending_entry->set_should_replace_entry(true);
  pending_entry->set_should_clear_history_list(true);
  EXPECT_TRUE(pending_entry->GetPostData());
  EXPECT_TRUE(pending_entry->is_renderer_initiated());
  EXPECT_TRUE(pending_entry->should_replace_entry());
  EXPECT_TRUE(pending_entry->should_clear_history_list());

  // Fake a commit response.
  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigateWithReplacement(entry_id, true, url1);

  // Certain values that are only used for pending entries get reset after
  // commit.
  NavigationEntryImpl* committed_entry = controller.GetLastCommittedEntry();
  EXPECT_FALSE(committed_entry->GetPostData());
  EXPECT_FALSE(committed_entry->is_renderer_initiated());
  EXPECT_FALSE(committed_entry->should_replace_entry());
  EXPECT_FALSE(committed_entry->should_clear_history_list());
}

namespace {
void SetRedirects(const std::vector<GURL>& redirects,
                  FrameHostMsg_DidCommitProvisionalLoad_Params* params) {
  params->redirects = redirects;
}
}

// Test that Redirects are preserved after a commit.
TEST_F(NavigationControllerTest, RedirectsAreNotResetByCommit) {
  NavigationControllerImpl& controller = controller_impl();
  const GURL url1("http://foo1");
  const GURL url2("http://foo2");
  controller.LoadURL(
      url1, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  int entry_id = controller.GetPendingEntry()->GetUniqueID();

  // Set up some redirect values.
  std::vector<GURL> redirects;
  redirects.push_back(url2);
  auto set_redirects_callback = base::Bind(SetRedirects, redirects);

  // Set redirects on the pending entry.
  NavigationEntryImpl* pending_entry = controller.GetPendingEntry();
  pending_entry->SetRedirectChain(redirects);
  EXPECT_EQ(1U, pending_entry->GetRedirectChain().size());
  EXPECT_EQ(url2, pending_entry->GetRedirectChain()[0]);

  // Normal navigation will preserve redirects in the committed entry.
  main_test_rfh()->PrepareForCommitWithServerRedirect(url2);
  main_test_rfh()->SendNavigateWithModificationCallback(
      entry_id, true, url1, std::move(set_redirects_callback));
  NavigationEntryImpl* committed_entry = controller.GetLastCommittedEntry();
  ASSERT_EQ(1U, committed_entry->GetRedirectChain().size());
  EXPECT_EQ(url2, committed_entry->GetRedirectChain()[0]);
}

// Tests that webkit preferences are updated when user agent override changes.
TEST_F(NavigationControllerTest, GoBackWithUserAgentOverrideChange) {
  NavigationControllerImpl& controller = controller_impl();

  // Set up a simple NavigationEntry stack of two pages.
  const GURL url1("http://foo/1");
  const GURL url2("http://foo/2");

  controller.LoadURL(url1, Referrer(), ui::PAGE_TRANSITION_TYPED,
                     std::string());
  int entry_id = controller.GetPendingEntry()->GetUniqueID();
  EXPECT_FALSE(controller.GetPendingEntry()->GetIsOverridingUserAgent());
  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigate(entry_id, true, url1);

  controller.LoadURL(url2, Referrer(), ui::PAGE_TRANSITION_TYPED,
                     std::string());
  entry_id = controller.GetPendingEntry()->GetUniqueID();
  EXPECT_FALSE(controller.GetPendingEntry()->GetIsOverridingUserAgent());
  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigate(entry_id, true, url2);

  // Simulate the behavior of "Request Desktop Site" being checked in
  // NavigationControllerAndroid::SetUseDesktopUserAgent.
  controller.GetVisibleEntry()->SetIsOverridingUserAgent(true);
  controller.Reload(ReloadType::ORIGINAL_REQUEST_URL, true);
  EXPECT_TRUE(controller.GetLastCommittedEntry()->GetIsOverridingUserAgent());

  // Test that OnWebkitPreferencesChanged is called when going back to propagate
  // change in viewport_meta WebSetting.
  int change_counter = 0;
  test_rvh()->set_webkit_preferences_changed_counter(&change_counter);

  controller.GoBack();
  entry_id = controller.GetPendingEntry()->GetUniqueID();
  EXPECT_FALSE(controller.GetPendingEntry()->GetIsOverridingUserAgent());
  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigateWithTransition(
      entry_id, false, url1, controller.GetPendingEntry()->GetTransitionType());

  EXPECT_EQ(1, change_counter);
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

  controller.GoBack();
  int entry_id = controller.GetPendingEntry()->GetUniqueID();
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

  TestRenderFrameHost* navigating_rfh = AreAllSitesIsolatedForTesting()
                                            ? contents()->GetPendingMainFrame()
                                            : contents()->GetMainFrame();
  navigating_rfh->PrepareForCommit();
  navigating_rfh->SendNavigate(entry_id, false, url2);
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

  const GURL url1("http://foo/1");
  const GURL url2("http://foo/2");
  const GURL url3("http://foo/3");

  controller.LoadURL(
      url1, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  int entry1_id = controller.GetPendingEntry()->GetUniqueID();
  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigate(entry1_id, true, url1);
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;
  entry1_id = controller.GetLastCommittedEntry()->GetUniqueID();

  controller.LoadURL(
      url2, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  int entry_id = controller.GetPendingEntry()->GetUniqueID();
  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigate(entry_id, true, url2);
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;

  controller.GoBack();
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

  main_test_rfh()->PrepareForCommitWithServerRedirect(url3);
  main_test_rfh()->SendNavigateWithTransition(
      entry1_id, true, url3, controller.GetPendingEntry()->GetTransitionType());
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;

  // The back navigation resulted in a completely new navigation.
  // TODO(darin): perhaps this behavior will be confusing to users?
  EXPECT_EQ(controller.GetEntryCount(), 3);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), 2);
  EXPECT_EQ(controller.GetPendingEntryIndex(), -1);
  EXPECT_TRUE(controller.GetLastCommittedEntry());
  EXPECT_FALSE(controller.GetPendingEntry());
  EXPECT_TRUE(controller.CanGoBack());
  EXPECT_FALSE(controller.CanGoForward());
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

  // controller.LoadURL(kUrl2, ui::PAGE_TRANSITION_TYPED);
  NavigationSimulator::NavigateAndCommitFromDocument(kUrl2, main_test_rfh());
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;

  // Now start a new pending navigation and go back before it commits.
  controller.LoadURL(
      kUrl3, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
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

  main_test_rfh()->SendRendererInitiatedNavigationRequest(url1, true);
  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigate(0, true, url1);
  NavigationEntry* entry1 = controller.GetLastCommittedEntry();
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;

  main_test_rfh()->SendRendererInitiatedNavigationRequest(url2, true);
  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigate(0, true, url2);
  NavigationEntry* entry2 = controller.GetLastCommittedEntry();
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;

  controller.GoBack();
  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigateWithTransition(
      entry1->GetUniqueID(), false, url1,
      controller.GetPendingEntry()->GetTransitionType());
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;

  controller.GoForward();

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

  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigateWithTransition(
      entry2->GetUniqueID(), false, url2,
      controller.GetPendingEntry()->GetTransitionType());
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

  main_test_rfh()->SendRendererInitiatedNavigationRequest(url1, true);
  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigate(0, true, url1);
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  NavigationEntry* entry1 = controller.GetLastCommittedEntry();
  navigation_entry_committed_counter_ = 0;
  main_test_rfh()->SendRendererInitiatedNavigationRequest(url2, true);
  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigate(0, true, url2);
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  NavigationEntry* entry2 = controller.GetLastCommittedEntry();
  navigation_entry_committed_counter_ = 0;

  controller.GoBack();
  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigateWithTransition(
      entry1->GetUniqueID(), false, url1,
      controller.GetPendingEntry()->GetTransitionType());
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;

  controller.GoForward();
  EXPECT_EQ(0U, navigation_list_pruned_counter_);

  // Should now have a pending navigation to go forward.
  EXPECT_EQ(controller.GetEntryCount(), 2);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), 0);
  EXPECT_EQ(controller.GetPendingEntryIndex(), 1);
  EXPECT_TRUE(controller.GetLastCommittedEntry());
  EXPECT_TRUE(controller.GetPendingEntry());
  EXPECT_TRUE(controller.CanGoBack());
  EXPECT_FALSE(controller.CanGoForward());

  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigate(entry2->GetUniqueID(), true, url3);
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;
  EXPECT_EQ(1U, navigation_list_pruned_counter_);

  EXPECT_EQ(controller.GetEntryCount(), 2);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), 1);
  EXPECT_EQ(controller.GetPendingEntryIndex(), -1);
  EXPECT_TRUE(controller.GetLastCommittedEntry());
  EXPECT_FALSE(controller.GetPendingEntry());
  EXPECT_TRUE(controller.CanGoBack());
  EXPECT_FALSE(controller.CanGoForward());
}

// Two consecutive navigations for the same URL entered in should be considered
// as SAME_PAGE navigation even when we are redirected to some other page.
TEST_F(NavigationControllerTest, Redirect) {
  NavigationControllerImpl& controller = controller_impl();

  const GURL url1("http://foo1");
  const GURL url2("http://foo2");  // Redirection target

  // First request.
  controller.LoadURL(
      url1, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  int entry_id = controller.GetPendingEntry()->GetUniqueID();

  EXPECT_EQ(0U, navigation_entry_changed_counter_);
  EXPECT_EQ(0U, navigation_list_pruned_counter_);

  FrameHostMsg_DidCommitProvisionalLoad_Params params;
  params.nav_entry_id = entry_id;
  params.did_create_new_entry = true;
  params.url = url2;
  params.transition = ui::PAGE_TRANSITION_SERVER_REDIRECT;
  params.redirects.push_back(GURL("http://foo1"));
  params.redirects.push_back(GURL("http://foo2"));
  params.should_update_history = false;
  params.gesture = NavigationGestureAuto;
  params.method = "GET";
  params.page_state = PageState::CreateFromURL(url2);

  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigateWithParams(&params, false);
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;

  // Second request.
  controller.LoadURL(
      url1, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  entry_id = controller.GetPendingEntry()->GetUniqueID();

  EXPECT_TRUE(controller.GetPendingEntry());
  EXPECT_EQ(controller.GetPendingEntryIndex(), -1);
  EXPECT_EQ(url1, controller.GetVisibleEntry()->GetURL());

  params.nav_entry_id = entry_id;
  params.did_create_new_entry = false;

  EXPECT_EQ(0U, navigation_entry_changed_counter_);
  EXPECT_EQ(0U, navigation_list_pruned_counter_);
  LoadCommittedDetailsObserver observer(contents());
  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigateWithParams(&params, false);
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;

  EXPECT_EQ(NAVIGATION_TYPE_SAME_PAGE, observer.navigation_type());
  EXPECT_EQ(controller.GetEntryCount(), 1);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), 0);
  EXPECT_TRUE(controller.GetLastCommittedEntry());
  EXPECT_EQ(controller.GetPendingEntryIndex(), -1);
  EXPECT_FALSE(controller.GetPendingEntry());
  EXPECT_EQ(url2, controller.GetVisibleEntry()->GetURL());

  EXPECT_FALSE(controller.CanGoBack());
  EXPECT_FALSE(controller.CanGoForward());
}

// Similar to Redirect above, but the first URL is requested by POST,
// the second URL is requested by GET. NavigationEntry::has_post_data_
// must be cleared. http://crbug.com/21245
TEST_F(NavigationControllerTest, PostThenRedirect) {
  NavigationControllerImpl& controller = controller_impl();

  const GURL url1("http://foo1");
  const GURL url2("http://foo2");  // Redirection target

  // First request as POST.
  controller.LoadURL(
      url1, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  int entry_id = controller.GetPendingEntry()->GetUniqueID();
  controller.GetVisibleEntry()->SetHasPostData(true);

  EXPECT_EQ(0U, navigation_entry_changed_counter_);
  EXPECT_EQ(0U, navigation_list_pruned_counter_);

  FrameHostMsg_DidCommitProvisionalLoad_Params params;
  params.nav_entry_id = entry_id;
  params.did_create_new_entry = true;
  params.url = url2;
  params.transition = ui::PAGE_TRANSITION_SERVER_REDIRECT;
  params.redirects.push_back(GURL("http://foo1"));
  params.redirects.push_back(GURL("http://foo2"));
  params.should_update_history = false;
  params.gesture = NavigationGestureAuto;
  params.method = "POST";
  params.page_state = PageState::CreateFromURL(url2);

  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigateWithParams(&params, false);
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;

  // Second request.
  controller.LoadURL(
      url1, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  entry_id = controller.GetPendingEntry()->GetUniqueID();

  EXPECT_TRUE(controller.GetPendingEntry());
  EXPECT_EQ(controller.GetPendingEntryIndex(), -1);
  EXPECT_EQ(url1, controller.GetVisibleEntry()->GetURL());

  params.nav_entry_id = entry_id;
  params.did_create_new_entry = false;
  params.method = "GET";

  EXPECT_EQ(0U, navigation_entry_changed_counter_);
  EXPECT_EQ(0U, navigation_list_pruned_counter_);
  LoadCommittedDetailsObserver observer(contents());
  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigateWithParams(&params, false);
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;

  EXPECT_EQ(NAVIGATION_TYPE_SAME_PAGE, observer.navigation_type());
  EXPECT_EQ(controller.GetEntryCount(), 1);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), 0);
  EXPECT_TRUE(controller.GetLastCommittedEntry());
  EXPECT_EQ(controller.GetPendingEntryIndex(), -1);
  EXPECT_FALSE(controller.GetPendingEntry());
  EXPECT_EQ(url2, controller.GetVisibleEntry()->GetURL());
  EXPECT_FALSE(controller.GetVisibleEntry()->GetHasPostData());

  EXPECT_FALSE(controller.CanGoBack());
  EXPECT_FALSE(controller.CanGoForward());
}

// A redirect right off the bat should be a NEW_PAGE.
TEST_F(NavigationControllerTest, ImmediateRedirect) {
  NavigationControllerImpl& controller = controller_impl();

  const GURL url1("http://foo1");
  const GURL url2("http://foo2");  // Redirection target

  // First request
  controller.LoadURL(
      url1, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  int entry_id = controller.GetPendingEntry()->GetUniqueID();

  EXPECT_TRUE(controller.GetPendingEntry());
  EXPECT_EQ(controller.GetPendingEntryIndex(), -1);
  EXPECT_EQ(url1, controller.GetVisibleEntry()->GetURL());

  FrameHostMsg_DidCommitProvisionalLoad_Params params;
  params.nav_entry_id = entry_id;
  params.did_create_new_entry = true;
  params.url = url2;
  params.transition = ui::PAGE_TRANSITION_SERVER_REDIRECT;
  params.redirects.push_back(GURL("http://foo1"));
  params.redirects.push_back(GURL("http://foo2"));
  params.should_update_history = false;
  params.gesture = NavigationGestureAuto;
  params.method = "GET";
  params.page_state = PageState::CreateFromURL(url2);

  LoadCommittedDetailsObserver observer(contents());

  EXPECT_EQ(0U, navigation_entry_changed_counter_);
  EXPECT_EQ(0U, navigation_list_pruned_counter_);
  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigateWithParams(&params, false);
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;

  EXPECT_EQ(NAVIGATION_TYPE_NEW_PAGE, observer.navigation_type());
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
// entry correctly turns a SAME_PAGE classification into an EXISTING_PAGE one.
//
// (This is a unit test rather than a browser test because it's not currently
// possible to force this sequence of events with a browser test.)
TEST_F(NavigationControllerTest,
       NavigationTypeClassification_ExistingPageRace) {
  NavigationControllerImpl& controller = controller_impl();
  const GURL url1("http://foo1");
  const GURL url2("http://foo2");

  // Start with a loaded page.
  NavigationSimulator::NavigateAndCommitFromDocument(url1, main_test_rfh());
  EXPECT_EQ(nullptr, controller_impl().GetPendingEntry());

  // Start a load of the same page again.
  controller.LoadURL(
      url1, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  int entry_id1 = controller.GetPendingEntry()->GetUniqueID();

  // Immediately start loading a different page...
  controller.LoadURL(
      url2, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  int entry_id2 = controller.GetPendingEntry()->GetUniqueID();
  EXPECT_NE(entry_id1, entry_id2);

  // ... and now the renderer sends a commit for the first navigation.
  FrameHostMsg_DidCommitProvisionalLoad_Params params;
  params.nav_entry_id = entry_id1;
  params.intended_as_new_entry = true;
  params.did_create_new_entry = false;
  params.url = url1;
  params.transition = ui::PAGE_TRANSITION_TYPED;
  params.page_state = PageState::CreateFromURL(url1);

  LoadCommittedDetailsObserver observer(contents());

  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigateWithParams(&params, false);
  EXPECT_EQ(NAVIGATION_TYPE_EXISTING_PAGE, observer.navigation_type());
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
  std::string unique_name("uniqueName0");
  main_test_rfh()->OnCreateChildFrame(
      process()->GetNextRoutingID(),
      TestRenderFrameHost::CreateStubInterfaceProviderRequest(),
      blink::WebTreeScopeType::kDocument, std::string(), unique_name, false,
      base::UnguessableToken::Create(), blink::FramePolicy(),
      FrameOwnerProperties(), blink::FrameOwnerElementType::kIframe);
  TestRenderFrameHost* subframe = static_cast<TestRenderFrameHost*>(
      contents()->GetFrameTree()->root()->child_at(0)->current_frame_host());
  const GURL subframe_url("http://foo1/subframe");
  {
    FrameHostMsg_DidCommitProvisionalLoad_Params params;
    params.nav_entry_id = 0;
    params.did_create_new_entry = false;
    params.url = subframe_url;
    params.transition = ui::PAGE_TRANSITION_AUTO_SUBFRAME;
    params.should_update_history = false;
    params.gesture = NavigationGestureUser;
    params.method = "GET";
    params.page_state = PageState::CreateFromURL(subframe_url);

    // Navigating should do nothing.
    subframe->SendRendererInitiatedNavigationRequest(subframe_url, false);
    subframe->PrepareForCommit();
    subframe->SendNavigateWithParams(&params, false);

    // We notify of a PageState update here rather than during UpdateState for
    // auto subframe navigations.
    EXPECT_EQ(1u, navigation_entry_changed_counter_);
  }

  // Now do a new navigation in the frame.
  const GURL url2("http://foo2");
  FrameHostMsg_DidCommitProvisionalLoad_Params params;
  params.nav_entry_id = 0;
  params.did_create_new_entry = true;
  params.url = url2;
  params.transition = ui::PAGE_TRANSITION_MANUAL_SUBFRAME;
  params.should_update_history = false;
  params.gesture = NavigationGestureUser;
  params.method = "GET";
  params.page_state = PageState::CreateFromURL(url2);

  LoadCommittedDetailsObserver observer(contents());
  subframe->SendRendererInitiatedNavigationRequest(url2, true);
  subframe->PrepareForCommit();
  subframe->SendNavigateWithParams(&params, false);
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;
  EXPECT_EQ(url1, observer.previous_url());
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
      TestRenderFrameHost::CreateStubInterfaceProviderRequest(),
      blink::WebTreeScopeType::kDocument, std::string(), unique_name0, false,
      base::UnguessableToken::Create(), blink::FramePolicy(),
      FrameOwnerProperties(), kOwnerType);
  TestRenderFrameHost* subframe = static_cast<TestRenderFrameHost*>(
      contents()->GetFrameTree()->root()->child_at(0)->current_frame_host());
  const GURL url2("http://foo/2");
  {
    FrameHostMsg_DidCommitProvisionalLoad_Params params;
    params.nav_entry_id = 0;
    params.did_create_new_entry = false;
    params.url = url2;
    params.transition = ui::PAGE_TRANSITION_AUTO_SUBFRAME;
    params.should_update_history = false;
    params.gesture = NavigationGestureUser;
    params.method = "GET";
    params.page_state = PageState::CreateFromURL(url2);

    // Navigating should do nothing.
    subframe->SendRendererInitiatedNavigationRequest(url2, false);
    subframe->PrepareForCommit();
    subframe->SendNavigateWithParams(&params, false);

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
      TestRenderFrameHost::CreateStubInterfaceProviderRequest(),
      blink::WebTreeScopeType::kDocument, std::string(), unique_name1, false,
      base::UnguessableToken::Create(), blink::FramePolicy(),
      FrameOwnerProperties(), kOwnerType);
  TestRenderFrameHost* subframe2 = static_cast<TestRenderFrameHost*>(
      contents()->GetFrameTree()->root()->child_at(1)->current_frame_host());
  const GURL url3("http://foo/3");
  {
    FrameHostMsg_DidCommitProvisionalLoad_Params params;
    params.nav_entry_id = 0;
    params.did_create_new_entry = false;
    params.url = url3;
    params.transition = ui::PAGE_TRANSITION_AUTO_SUBFRAME;
    params.should_update_history = false;
    params.gesture = NavigationGestureUser;
    params.method = "GET";
    params.page_state = PageState::CreateFromURL(url3);

    // Navigating should do nothing.
    subframe2->SendRendererInitiatedNavigationRequest(url3, false);
    subframe2->PrepareForCommit();
    subframe2->SendNavigateWithParams(&params, false);

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
      TestRenderFrameHost::CreateStubInterfaceProviderRequest(),
      blink::WebTreeScopeType::kDocument, std::string(), unique_name2, false,
      base::UnguessableToken::Create(), blink::FramePolicy(),
      FrameOwnerProperties(), kOwnerType);
  TestRenderFrameHost* subframe3 =
      static_cast<TestRenderFrameHost*>(contents()
                                            ->GetFrameTree()
                                            ->root()
                                            ->child_at(0)
                                            ->child_at(0)
                                            ->current_frame_host());
  const GURL url4("http://foo/4");
  {
    FrameHostMsg_DidCommitProvisionalLoad_Params params;
    params.nav_entry_id = 0;
    params.did_create_new_entry = false;
    params.url = url4;
    params.transition = ui::PAGE_TRANSITION_AUTO_SUBFRAME;
    params.should_update_history = false;
    params.gesture = NavigationGestureUser;
    params.method = "GET";
    params.page_state = PageState::CreateFromURL(url4);

    // Navigating should do nothing.
    subframe3->SendRendererInitiatedNavigationRequest(url4, false);
    subframe3->PrepareForCommit();
    subframe3->SendNavigateWithParams(&params, false);

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
      TestRenderFrameHost::CreateStubInterfaceProviderRequest(),
      blink::WebTreeScopeType::kDocument, std::string(), unique_name, false,
      base::UnguessableToken::Create(), blink::FramePolicy(),
      FrameOwnerProperties(), blink::FrameOwnerElementType::kIframe);
  TestRenderFrameHost* subframe = static_cast<TestRenderFrameHost*>(
      contents()->GetFrameTree()->root()->child_at(0)->current_frame_host());
  const GURL subframe_url("http://foo1/subframe");

  // Compute the sequence number assigned by Blink.
  int64_t item_sequence_number1 = GenerateSequenceNumber();
  int64_t document_sequence_number1 = GenerateSequenceNumber();

  {
    FrameHostMsg_DidCommitProvisionalLoad_Params params;
    params.nav_entry_id = 0;
    params.did_create_new_entry = false;
    params.url = subframe_url;
    params.transition = ui::PAGE_TRANSITION_AUTO_SUBFRAME;
    params.should_update_history = false;
    params.gesture = NavigationGestureUser;
    params.method = "GET";
    params.page_state = PageState::CreateForTestingWithSequenceNumbers(
        subframe_url, item_sequence_number1, document_sequence_number1);
    params.item_sequence_number = item_sequence_number1;
    params.document_sequence_number = document_sequence_number1;

    // Navigating should do nothing.
    subframe->SendRendererInitiatedNavigationRequest(subframe_url, false);
    subframe->PrepareForCommit();
    subframe->SendNavigateWithParams(&params, false);

    // We notify of a PageState update here rather than during UpdateState for
    // auto subframe navigations.
    EXPECT_EQ(1u, navigation_entry_changed_counter_);
  }

  // First manual subframe navigation.
  const GURL url2("http://foo2");
  int64_t item_sequence_number2 = GenerateSequenceNumber();
  int64_t document_sequence_number2 = GenerateSequenceNumber();
  FrameHostMsg_DidCommitProvisionalLoad_Params params;
  params.nav_entry_id = 0;
  params.did_create_new_entry = true;
  params.url = url2;
  params.transition = ui::PAGE_TRANSITION_MANUAL_SUBFRAME;
  params.should_update_history = false;
  params.gesture = NavigationGestureUser;
  params.method = "GET";
  params.page_state = PageState::CreateForTestingWithSequenceNumbers(
      url2, item_sequence_number2, document_sequence_number2);
  params.item_sequence_number = item_sequence_number2;
  params.document_sequence_number = document_sequence_number2;

  // This should generate a new entry.
  subframe->SendRendererInitiatedNavigationRequest(url2, false);
  subframe->PrepareForCommit();
  subframe->SendNavigateWithParams(&params, false);
  NavigationEntryImpl* entry2 = controller.GetLastCommittedEntry();
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;
  EXPECT_EQ(2, controller.GetEntryCount());

  // The entry should have a subframe FrameNavigationEntry.
  ASSERT_EQ(1U, entry2->root_node()->children.size());
  EXPECT_EQ(url2, entry2->root_node()->children[0]->frame_entry->url());

  // Second manual subframe navigation should also make a new entry.
  const GURL url3("http://foo3");
  int64_t item_sequence_number3 = GenerateSequenceNumber();
  int64_t document_sequence_number3 = GenerateSequenceNumber();
  params.nav_entry_id = 0;
  params.did_create_new_entry = true;
  params.url = url3;
  params.transition = ui::PAGE_TRANSITION_MANUAL_SUBFRAME;
  params.item_sequence_number = item_sequence_number3;
  params.document_sequence_number = document_sequence_number3;
  params.page_state = PageState::CreateForTestingWithSequenceNumbers(
    url3, item_sequence_number3, document_sequence_number3);
  subframe->SendRendererInitiatedNavigationRequest(url3, false);
  subframe->PrepareForCommit();
  subframe->SendNavigateWithParams(&params, false);
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;
  NavigationEntryImpl* entry3 = controller.GetLastCommittedEntry();
  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(2, controller.GetCurrentEntryIndex());

  // The entry should have a subframe FrameNavigationEntry.
  ASSERT_EQ(1U, entry3->root_node()->children.size());
  EXPECT_EQ(url3, entry3->root_node()->children[0]->frame_entry->url());

  // Go back one.
  controller.GoBack();
  params.nav_entry_id = entry2->GetUniqueID();
  params.did_create_new_entry = false;
  params.url = url2;
  params.transition = ui::PAGE_TRANSITION_AUTO_SUBFRAME;
  params.page_state = PageState::CreateForTestingWithSequenceNumbers(
    url2, item_sequence_number2, document_sequence_number2);
  params.item_sequence_number = item_sequence_number2;
  params.document_sequence_number = document_sequence_number2;
  subframe->PrepareForCommit();
  subframe->SendNavigateWithParams(&params, false);
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;
  EXPECT_EQ(entry2, controller.GetLastCommittedEntry());
  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetCurrentEntryIndex());
  EXPECT_EQ(-1, controller.GetPendingEntryIndex());
  EXPECT_FALSE(controller.GetPendingEntry());

  // Go back one more.
  controller.GoBack();
  params.nav_entry_id = entry1->GetUniqueID();
  params.did_create_new_entry = false;
  params.url = subframe_url;
  params.transition = ui::PAGE_TRANSITION_AUTO_SUBFRAME;
  params.page_state = PageState::CreateForTestingWithSequenceNumbers(
    subframe_url, item_sequence_number1, document_sequence_number1);
  params.item_sequence_number = item_sequence_number1;
  params.document_sequence_number = document_sequence_number1;
  subframe->PrepareForCommit();
  subframe->SendNavigateWithParams(&params, false);
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
  FrameHostMsg_DidCommitProvisionalLoad_Params self_params;
  self_params.nav_entry_id = 0;
  self_params.did_create_new_entry = true;
  self_params.should_replace_current_entry = true;
  self_params.url = url1;
  self_params.transition = ui::PAGE_TRANSITION_LINK;
  self_params.should_update_history = false;
  self_params.gesture = NavigationGestureUser;
  self_params.method = "GET";
  self_params.item_sequence_number = GenerateSequenceNumber();
  self_params.document_sequence_number = GenerateSequenceNumber();
  self_params.page_state = PageState::CreateForTestingWithSequenceNumbers(
      url1, self_params.item_sequence_number,
      self_params.document_sequence_number);

  LoadCommittedDetailsObserver observer(contents());
  main_test_rfh()->SendRendererInitiatedNavigationRequest(url1, false);
  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigateWithParams(&self_params, false);
  NavigationEntry* entry1 = controller.GetLastCommittedEntry();
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;
  EXPECT_FALSE(observer.is_same_document());
  EXPECT_TRUE(observer.did_replace_entry());
  EXPECT_EQ(1, controller.GetEntryCount());

  // Fragment navigation to a new page.
  const GURL url2("http://foo#a");
  FrameHostMsg_DidCommitProvisionalLoad_Params params;
  params.nav_entry_id = 0;
  params.did_create_new_entry = true;
  params.url = url2;
  params.transition = ui::PAGE_TRANSITION_LINK;
  params.should_update_history = false;
  params.gesture = NavigationGestureUser;
  params.method = "GET";
  params.item_sequence_number = GenerateSequenceNumber();
  params.document_sequence_number = self_params.document_sequence_number;
  params.page_state = PageState::CreateForTestingWithSequenceNumbers(
      url2, params.item_sequence_number, params.document_sequence_number);

  // This should generate a new entry.
  main_test_rfh()->SendNavigateWithParams(&params, true);
  NavigationEntry* entry2 = controller.GetLastCommittedEntry();
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;
  EXPECT_TRUE(observer.is_same_document());
  EXPECT_FALSE(observer.did_replace_entry());
  EXPECT_EQ(2, controller.GetEntryCount());

  // Go back one.
  FrameHostMsg_DidCommitProvisionalLoad_Params back_params(self_params);
  controller.GoBack();
  back_params.nav_entry_id = entry1->GetUniqueID();
  back_params.did_create_new_entry = false;
  main_test_rfh()->SendNavigateWithParams(&back_params, true);
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;
  EXPECT_TRUE(observer.is_same_document());
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(0, controller.GetCurrentEntryIndex());
  EXPECT_EQ(back_params.url, controller.GetVisibleEntry()->GetURL());

  // Go forward.
  FrameHostMsg_DidCommitProvisionalLoad_Params forward_params(params);
  controller.GoForward();
  forward_params.nav_entry_id = entry2->GetUniqueID();
  forward_params.did_create_new_entry = false;
  main_test_rfh()->SendNavigateWithParams(&forward_params, true);
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetCurrentEntryIndex());
  EXPECT_EQ(forward_params.url,
            controller.GetVisibleEntry()->GetURL());

  // Now go back and forward again. This is to work around a bug where we would
  // compare the incoming URL with the last committed entry rather than the
  // one identified by an existing page ID. This would result in the second URL
  // losing the reference fragment when you navigate away from it and then back.
  controller.GoBack();
  main_test_rfh()->SendNavigateWithParams(&back_params, false);
  controller.GoForward();
  main_test_rfh()->SendNavigateWithParams(&forward_params, false);
  EXPECT_EQ(forward_params.url,
            controller.GetVisibleEntry()->GetURL());

  // Finally, navigate to an unrelated URL to make sure same_document is not
  // sticky.
  const GURL url3("http://bar");
  params.nav_entry_id = 0;
  params.did_create_new_entry = true;
  params.url = url3;
  params.item_sequence_number = 0;
  params.document_sequence_number = 0;
  params.page_state = PageState::CreateFromURL(url3);
  navigation_entry_committed_counter_ = 0;
  main_test_rfh()->SendRendererInitiatedNavigationRequest(url3, false);
  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigateWithParams(&params, false);
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
  FrameHostMsg_DidCommitProvisionalLoad_Params params;
  params.nav_entry_id = 0;
  params.did_create_new_entry = false;
  params.should_replace_current_entry = true;
  params.url = url2;
  params.transition = ui::PAGE_TRANSITION_LINK;
  params.should_update_history = false;
  params.gesture = NavigationGestureUser;
  params.method = "GET";
  params.page_state = PageState::CreateFromURL(url2);

  // This should NOT generate a new entry, nor prune the list.
  LoadCommittedDetailsObserver observer(contents());
  main_test_rfh()->SendNavigateWithParams(&params, true);
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;
  EXPECT_TRUE(observer.is_same_document());
  EXPECT_TRUE(observer.did_replace_entry());
  EXPECT_EQ(1, controller.GetEntryCount());
}

// Tests for http://crbug.com/40395
// Simulates this:
//   <script>
//     window.location.replace("#a");
//     window.location='http://foo3/';
//   </script>
TEST_F(NavigationControllerTest, ClientRedirectAfterSameDocumentNavigation) {
  NavigationControllerImpl& controller = controller_impl();

  // Load an initial page.
  {
    const GURL url("http://foo/");
    NavigationSimulator::NavigateAndCommitFromDocument(url, main_test_rfh());
    EXPECT_EQ(1U, navigation_entry_committed_counter_);
    navigation_entry_committed_counter_ = 0;
  }

  // Navigate to a new page.
  {
    const GURL url("http://foo2/");
    NavigationSimulator::NavigateAndCommitFromDocument(url, main_test_rfh());
    EXPECT_EQ(1U, navigation_entry_committed_counter_);
    navigation_entry_committed_counter_ = 0;
  }

  // Perform same-document navigation using location.replace.
  {
    const GURL url("http://foo2/#a");
    FrameHostMsg_DidCommitProvisionalLoad_Params params;
    params.nav_entry_id = 0;
    params.did_create_new_entry = false;
    params.should_replace_current_entry = true;
    params.url = url;
    params.transition = ui::PAGE_TRANSITION_LINK;
    params.redirects.push_back(url);
    params.should_update_history = true;
    params.gesture = NavigationGestureAuto;
    params.method = "GET";
    params.page_state = PageState::CreateFromURL(url);

    // This should NOT generate a new entry, nor prune the list.
    LoadCommittedDetailsObserver observer(contents());
    main_test_rfh()->SendNavigateWithParams(&params, true);
    EXPECT_EQ(1U, navigation_entry_committed_counter_);
    navigation_entry_committed_counter_ = 0;
    EXPECT_TRUE(observer.is_same_document());
    EXPECT_TRUE(observer.did_replace_entry());
    EXPECT_EQ(2, controller.GetEntryCount());
  }

  // Perform a client redirect to a new page.
  {
    const GURL url("http://foo3/");
    FrameHostMsg_DidCommitProvisionalLoad_Params params;
    params.nav_entry_id = 0;
    params.did_create_new_entry = true;
    params.url = url;
    params.transition = ui::PAGE_TRANSITION_CLIENT_REDIRECT;
    params.redirects.push_back(GURL("http://foo2/#a"));
    params.redirects.push_back(url);
    params.should_update_history = true;
    params.gesture = NavigationGestureAuto;
    params.method = "GET";
    params.page_state = PageState::CreateFromURL(url);

    // This SHOULD generate a new entry.
    LoadCommittedDetailsObserver observer(contents());
    main_test_rfh()->SendRendererInitiatedNavigationRequest(url, false);
    main_test_rfh()->PrepareForCommit();
    main_test_rfh()->SendNavigateWithParams(&params, false);
    EXPECT_EQ(1U, navigation_entry_committed_counter_);
    navigation_entry_committed_counter_ = 0;
    EXPECT_FALSE(observer.is_same_document());
    EXPECT_EQ(3, controller.GetEntryCount());
  }

  // Verify that BACK brings us back to http://foo2/.
  {
    const GURL url("http://foo2/");
    controller.GoBack();
    int entry_id = controller.GetPendingEntry()->GetUniqueID();
    main_test_rfh()->PrepareForCommit();
    main_test_rfh()->SendNavigate(entry_id, false, url);
    EXPECT_EQ(1U, navigation_entry_committed_counter_);
    navigation_entry_committed_counter_ = 0;
    EXPECT_EQ(url, controller.GetVisibleEntry()->GetURL());
  }
}

TEST_F(NavigationControllerTest, PushStateWithoutPreviousEntry) {
  ASSERT_FALSE(controller_impl().GetLastCommittedEntry());
  FrameHostMsg_DidCommitProvisionalLoad_Params params;
  GURL url("http://foo");
  params.nav_entry_id = 0;
  params.did_create_new_entry = true;
  params.url = url;
  params.page_state = PageState::CreateFromURL(url);
  main_test_rfh()->SendRendererInitiatedNavigationRequest(url, false);
  main_test_rfh()->PrepareForCommit();
  contents()->GetMainFrame()->SendNavigateWithParams(&params, true);
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
    controller.LoadURL(
        url, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
    int entry_id = controller.GetPendingEntry()->GetUniqueID();
    main_test_rfh()->PrepareForCommit();
    main_test_rfh()->SendNavigate(entry_id, true, url);
  }

  EXPECT_EQ(controller.GetEntryCount(), kMaxEntryCount);


  // Navigate some more.
  GURL url(base::StringPrintf("http://www.a.com/%d", url_index));
  controller.LoadURL(
      url, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  int entry_id = controller.GetPendingEntry()->GetUniqueID();
  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigate(entry_id, true, url);
  url_index++;

  // We should have got a pruned navigation.
  EXPECT_EQ(1U, navigation_list_pruned_counter_);
  EXPECT_TRUE(last_navigation_entry_pruned_details_.from_front);
  EXPECT_EQ(1, last_navigation_entry_pruned_details_.count);

  // We expect http://www.a.com/0 to be gone.
  EXPECT_EQ(controller.GetEntryCount(), kMaxEntryCount);
  EXPECT_EQ(controller.GetEntryAtIndex(0)->GetURL(),
            GURL("http://www.a.com/1"));

  // More navigations.
  for (int i = 0; i < 3; i++) {
    url = GURL(base::StringPrintf("http://www.a.com/%d", url_index));
    controller.LoadURL(
        url, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
    int entry_id = controller.GetPendingEntry()->GetUniqueID();
    main_test_rfh()->PrepareForCommit();
    main_test_rfh()->SendNavigate(entry_id, true, url);
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
  std::unique_ptr<NavigationEntry> entry =
      NavigationController::CreateNavigationEntry(
          url, Referrer(), ui::PAGE_TRANSITION_RELOAD, false, std::string(),
          browser_context(), nullptr /* blob_url_loader_factory */);
  entry->SetTitle(base::ASCIIToUTF16("Title"));
  entry->SetPageState(PageState::CreateFromEncodedData("state"));
  const base::Time timestamp = base::Time::Now();
  entry->SetTimestamp(timestamp);
  entries.push_back(std::move(entry));
  std::unique_ptr<WebContents> our_contents =
      WebContents::Create(WebContents::CreateParams(browser_context()));
  WebContentsImpl* raw_our_contents =
      static_cast<WebContentsImpl*>(our_contents.get());
  NavigationControllerImpl& our_controller = raw_our_contents->GetController();
  our_controller.Restore(0, RestoreType::LAST_SESSION_EXITED_CLEANLY, &entries);
  ASSERT_EQ(0u, entries.size());

  // Before navigating to the restored entry, it should have a restore_type
  // and no SiteInstance.
  ASSERT_EQ(1, our_controller.GetEntryCount());
  EXPECT_EQ(RestoreType::LAST_SESSION_EXITED_CLEANLY,
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
  FrameHostMsg_DidCommitProvisionalLoad_Params params;
  params.nav_entry_id = our_controller.GetPendingEntry()->GetUniqueID();
  params.did_create_new_entry = false;
  params.url = url;
  params.transition = ui::PAGE_TRANSITION_LINK;
  params.should_update_history = false;
  params.gesture = NavigationGestureUser;
  params.method = "GET";
  params.page_state = PageState::CreateFromURL(url);
  TestRenderFrameHost* main_rfh =
      static_cast<TestRenderFrameHost*>(raw_our_contents->GetMainFrame());
  main_rfh->PrepareForCommit();
  main_rfh->SendNavigateWithParams(&params, false);

  // There should be no longer any pending entry and one committed one. This
  // means that we were able to locate the entry, assign its site instance, and
  // commit it properly.
  EXPECT_EQ(1, our_controller.GetEntryCount());
  EXPECT_EQ(0, our_controller.GetLastCommittedEntryIndex());
  EXPECT_FALSE(our_controller.GetPendingEntry());
  EXPECT_EQ(
      url,
      our_controller.GetLastCommittedEntry()->site_instance()->GetSiteURL());
  EXPECT_EQ(RestoreType::NONE,
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
  std::unique_ptr<NavigationEntry> new_entry =
      NavigationController::CreateNavigationEntry(
          url, Referrer(), ui::PAGE_TRANSITION_RELOAD, false, std::string(),
          browser_context(), nullptr /* blob_url_loader_factory */);
  new_entry->SetTitle(base::ASCIIToUTF16("Title"));
  new_entry->SetPageState(PageState::CreateFromEncodedData("state"));
  entries.push_back(std::move(new_entry));
  std::unique_ptr<WebContents> our_contents =
      WebContents::Create(WebContents::CreateParams(browser_context()));
  WebContentsImpl* raw_our_contents =
      static_cast<WebContentsImpl*>(our_contents.get());
  NavigationControllerImpl& our_controller = raw_our_contents->GetController();
  our_controller.Restore(0, RestoreType::LAST_SESSION_EXITED_CLEANLY, &entries);
  ASSERT_EQ(0u, entries.size());

  // Ensure the RenderFrame is initialized before simulating events coming from
  // it.
  main_test_rfh()->InitializeRenderFrameIfNeeded();

  // Before navigating to the restored entry, it should have a restore_type
  // and no SiteInstance.
  NavigationEntry* entry = our_controller.GetEntryAtIndex(0);
  EXPECT_EQ(RestoreType::LAST_SESSION_EXITED_CLEANLY,
            our_controller.GetEntryAtIndex(0)->restore_type());
  EXPECT_FALSE(our_controller.GetEntryAtIndex(0)->site_instance());

  // After navigating, we should have one entry, and it should be "pending".
  EXPECT_TRUE(our_controller.NeedsReload());
  our_controller.LoadIfNecessary();
  EXPECT_EQ(1, our_controller.GetEntryCount());
  EXPECT_EQ(our_controller.GetEntryAtIndex(0),
            our_controller.GetPendingEntry());

  // This pending navigation may have caused a different navigation to fail,
  // which causes the pending entry to be cleared.
  FrameHostMsg_DidFailProvisionalLoadWithError_Params fail_load_params;
  fail_load_params.error_code = net::ERR_ABORTED;
  fail_load_params.error_description = base::string16();
  fail_load_params.url = url;
  fail_load_params.showing_repost_interstitial = false;
  main_test_rfh()->InitializeRenderFrameIfNeeded();
  main_test_rfh()->OnMessageReceived(
      FrameHostMsg_DidFailProvisionalLoadWithError(0,  // routing_id
                                                  fail_load_params));

  // Now the pending restored entry commits.
  FrameHostMsg_DidCommitProvisionalLoad_Params params;
  params.nav_entry_id = entry->GetUniqueID();
  params.did_create_new_entry = false;
  params.url = url;
  params.transition = ui::PAGE_TRANSITION_LINK;
  params.should_update_history = false;
  params.gesture = NavigationGestureUser;
  params.method = "GET";
  params.page_state = PageState::CreateFromURL(url);
  TestRenderFrameHost* main_rfh =
      static_cast<TestRenderFrameHost*>(raw_our_contents->GetMainFrame());
  main_rfh->PrepareForCommit();
  main_rfh->SendNavigateWithParams(&params, false);

  // There should be no pending entry and one committed one.
  EXPECT_EQ(1, our_controller.GetEntryCount());
  EXPECT_EQ(0, our_controller.GetLastCommittedEntryIndex());
  EXPECT_FALSE(our_controller.GetPendingEntry());
  EXPECT_EQ(
      url,
      our_controller.GetLastCommittedEntry()->site_instance()->GetSiteURL());
  EXPECT_EQ(RestoreType::NONE,
            our_controller.GetEntryAtIndex(0)->restore_type());
}

// Make sure that the page type and stuff is correct after an interstitial.
TEST_F(NavigationControllerTest, Interstitial) {
  NavigationControllerImpl& controller = controller_impl();
  // First navigate somewhere normal.
  const GURL url1("http://foo");
  controller.LoadURL(
      url1, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  int entry_id = controller.GetPendingEntry()->GetUniqueID();
  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigate(entry_id, true, url1);

  // Now navigate somewhere with an interstitial.
  const GURL url2("http://bar");
  controller.LoadURL(url2, Referrer(), ui::PAGE_TRANSITION_TYPED,
                     std::string());
  entry_id = controller.GetPendingEntry()->GetUniqueID();
  controller.GetPendingEntry()->set_page_type(PAGE_TYPE_INTERSTITIAL);

  // At this point the interstitial will be displayed and the load will still
  // be pending. If the user continues, the load will commit.
  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigate(entry_id, true, url2);

  // The page should be a normal page again.
  EXPECT_EQ(url2, controller.GetLastCommittedEntry()->GetURL());
  EXPECT_EQ(PAGE_TYPE_NORMAL,
            controller.GetLastCommittedEntry()->GetPageType());
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

  controller.LoadURL(
      url1, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  int entry_id = controller.GetPendingEntry()->GetUniqueID();
  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigate(entry_id, true, url1);
  controller.LoadURL(
      url2, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  entry_id = controller.GetPendingEntry()->GetUniqueID();
  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigate(entry_id, true, url2);
  controller.LoadURL(
      url3, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  entry_id = controller.GetPendingEntry()->GetUniqueID();
  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigate(entry_id, true, url3);
  controller.LoadURL(
      url4, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  entry_id = controller.GetPendingEntry()->GetUniqueID();
  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigate(entry_id, true, url4);
  controller.LoadURL(
      url5, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  entry_id = controller.GetPendingEntry()->GetUniqueID();
  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigate(entry_id, true, url5);

  // Try to remove the last entry.  Will fail because it is the current entry.
  EXPECT_FALSE(controller.RemoveEntryAtIndex(controller.GetEntryCount() - 1));
  EXPECT_EQ(5, controller.GetEntryCount());
  EXPECT_EQ(4, controller.GetLastCommittedEntryIndex());

  // Go back, but don't commit yet. Check that we can't delete the current
  // and pending entries.
  controller.GoBack();
  entry_id = controller.GetPendingEntry()->GetUniqueID();
  EXPECT_FALSE(controller.RemoveEntryAtIndex(controller.GetEntryCount() - 1));
  EXPECT_FALSE(controller.RemoveEntryAtIndex(controller.GetEntryCount() - 2));

  // Now commit and delete the last entry.
  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigateWithTransition(
      entry_id, false, url4, controller.GetPendingEntry()->GetTransitionType());
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

  controller.LoadURL(
      url1, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  int entry_id = controller.GetPendingEntry()->GetUniqueID();
  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigate(entry_id, true, url1);
  controller.LoadURL(
      url2, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  entry_id = controller.GetPendingEntry()->GetUniqueID();
  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigate(entry_id, true, url2);
  controller.LoadURL(
      url3, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  entry_id = controller.GetPendingEntry()->GetUniqueID();
  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigate(entry_id, true, url3);

  // Go back, but don't commit yet. Check that we can't delete the current
  // and pending entries.
  controller.GoBack();
  entry_id = controller.GetPendingEntry()->GetUniqueID();
  ui::PageTransition entry_transition =
      controller.GetPendingEntry()->GetTransitionType();
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
  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigateWithTransition(entry_id, false, url2,
                                              entry_transition);
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());
  EXPECT_FALSE(controller.GetPendingEntry());
}

// Tests the transient entry, making sure it goes away with all navigations.
TEST_F(NavigationControllerTest, TransientEntry) {
  NavigationControllerImpl& controller = controller_impl();

  const GURL url0("http://foo/0");
  const GURL url1("http://foo/1");
  const GURL url2("http://foo/2");
  const GURL url3("http://foo/3");
  const GURL url3_ref("http://foo/3#bar");
  const GURL url4("http://foo/4");
  const GURL transient_url("http://foo/transient");

  controller.LoadURL(
      url0, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  int entry_id = controller.GetPendingEntry()->GetUniqueID();
  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigate(entry_id, true, url0);
  controller.LoadURL(
      url1, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  entry_id = controller.GetPendingEntry()->GetUniqueID();
  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigate(entry_id, true, url1);

  navigation_entry_changed_counter_ = 0;
  navigation_list_pruned_counter_ = 0;

  // Adding a transient with no pending entry.
  std::unique_ptr<NavigationEntry> transient_entry(new NavigationEntryImpl);
  transient_entry->SetURL(transient_url);
  controller.SetTransientEntry(std::move(transient_entry));

  // We should not have received any notifications.
  EXPECT_EQ(0U, navigation_entry_changed_counter_);
  EXPECT_EQ(0U, navigation_list_pruned_counter_);

  // Check our state.
  EXPECT_EQ(transient_url, controller.GetVisibleEntry()->GetURL());
  EXPECT_EQ(controller.GetEntryCount(), 3);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), 1);
  EXPECT_EQ(controller.GetPendingEntryIndex(), -1);
  EXPECT_TRUE(controller.GetLastCommittedEntry());
  EXPECT_FALSE(controller.GetPendingEntry());
  EXPECT_TRUE(controller.CanGoBack());
  EXPECT_FALSE(controller.CanGoForward());

  // Navigate.
  controller.LoadURL(
      url2, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  entry_id = controller.GetPendingEntry()->GetUniqueID();
  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigate(entry_id, true, url2);

  // We should have navigated, transient entry should be gone.
  EXPECT_EQ(url2, controller.GetVisibleEntry()->GetURL());
  EXPECT_EQ(controller.GetEntryCount(), 3);

  // Add a transient again, then navigate with no pending entry this time.
  transient_entry.reset(new NavigationEntryImpl);
  transient_entry->SetURL(transient_url);
  controller.SetTransientEntry(std::move(transient_entry));
  EXPECT_EQ(transient_url, controller.GetVisibleEntry()->GetURL());
  main_test_rfh()->SendRendererInitiatedNavigationRequest(url3, true);
  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigate(0, true, url3);
  // Transient entry should be gone.
  EXPECT_EQ(url3, controller.GetVisibleEntry()->GetURL());
  EXPECT_EQ(controller.GetEntryCount(), 4);

  // Initiate a navigation, add a transient then commit navigation.
  controller.LoadURL(
      url4, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  entry_id = controller.GetPendingEntry()->GetUniqueID();
  transient_entry.reset(new NavigationEntryImpl);
  transient_entry->SetURL(transient_url);
  controller.SetTransientEntry(std::move(transient_entry));
  EXPECT_EQ(transient_url, controller.GetVisibleEntry()->GetURL());
  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigate(entry_id, true, url4);
  EXPECT_EQ(url4, controller.GetVisibleEntry()->GetURL());
  EXPECT_EQ(controller.GetEntryCount(), 5);

  // Add a transient and go back.  This should simply remove the transient.
  transient_entry.reset(new NavigationEntryImpl);
  transient_entry->SetURL(transient_url);
  controller.SetTransientEntry(std::move(transient_entry));
  EXPECT_EQ(transient_url, controller.GetVisibleEntry()->GetURL());
  EXPECT_TRUE(controller.CanGoBack());
  EXPECT_FALSE(controller.CanGoForward());
  controller.GoBack();
  // Transient entry should be gone.
  EXPECT_EQ(url4, controller.GetVisibleEntry()->GetURL());
  EXPECT_EQ(controller.GetEntryCount(), 5);

  // Suppose the page requested a history navigation backward.
  controller.GoToOffset(-1);
  entry_id = controller.GetPendingEntry()->GetUniqueID();
  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigateWithTransition(
      entry_id, false, url3, controller.GetPendingEntry()->GetTransitionType());

  // Add a transient and go to an entry before the current one.
  transient_entry.reset(new NavigationEntryImpl);
  transient_entry->SetURL(transient_url);
  controller.SetTransientEntry(std::move(transient_entry));
  EXPECT_EQ(transient_url, controller.GetVisibleEntry()->GetURL());
  controller.GoToIndex(1);
  entry_id = controller.GetPendingEntry()->GetUniqueID();
  // The navigation should have been initiated, transient entry should be gone.
  EXPECT_FALSE(controller.GetTransientEntry());
  EXPECT_EQ(url1, controller.GetPendingEntry()->GetURL());
  // Visible entry does not update for history navigations until commit.
  EXPECT_EQ(url3, controller.GetVisibleEntry()->GetURL());
  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigateWithTransition(
      entry_id, false, url1, controller.GetPendingEntry()->GetTransitionType());
  EXPECT_EQ(url1, controller.GetVisibleEntry()->GetURL());

  // Add a transient and go to an entry after the current one.
  transient_entry.reset(new NavigationEntryImpl);
  transient_entry->SetURL(transient_url);
  controller.SetTransientEntry(std::move(transient_entry));
  EXPECT_EQ(transient_url, controller.GetVisibleEntry()->GetURL());
  controller.GoToIndex(3);
  entry_id = controller.GetPendingEntry()->GetUniqueID();
  // The navigation should have been initiated, transient entry should be gone.
  // Because of the transient entry that is removed, going to index 3 makes us
  // land on url2 (which is visible after the commit).
  EXPECT_EQ(url2, controller.GetPendingEntry()->GetURL());
  EXPECT_EQ(url1, controller.GetVisibleEntry()->GetURL());
  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigateWithTransition(
      entry_id, false, url2, controller.GetPendingEntry()->GetTransitionType());
  EXPECT_EQ(url2, controller.GetVisibleEntry()->GetURL());

  // Add a transient and go forward.
  transient_entry.reset(new NavigationEntryImpl);
  transient_entry->SetURL(transient_url);
  controller.SetTransientEntry(std::move(transient_entry));
  EXPECT_EQ(transient_url, controller.GetVisibleEntry()->GetURL());
  EXPECT_TRUE(controller.CanGoForward());
  controller.GoForward();
  entry_id = controller.GetPendingEntry()->GetUniqueID();
  // We should have navigated, transient entry should be gone.
  EXPECT_FALSE(controller.GetTransientEntry());
  EXPECT_EQ(url3, controller.GetPendingEntry()->GetURL());
  EXPECT_EQ(url2, controller.GetVisibleEntry()->GetURL());
  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigateWithTransition(
      entry_id, false, url3, controller.GetPendingEntry()->GetTransitionType());
  EXPECT_EQ(url3, controller.GetVisibleEntry()->GetURL());

  // Add a transient and do an in-page navigation, replacing the current entry.
  transient_entry.reset(new NavigationEntryImpl);
  transient_entry->SetURL(transient_url);
  controller.SetTransientEntry(std::move(transient_entry));
  EXPECT_EQ(transient_url, controller.GetVisibleEntry()->GetURL());

  main_test_rfh()->SendNavigate(0, false, url3_ref);
  // Transient entry should be gone.
  EXPECT_FALSE(controller.GetTransientEntry());
  EXPECT_EQ(url3_ref, controller.GetVisibleEntry()->GetURL());

  // Ensure the URLs are correct.
  EXPECT_EQ(controller.GetEntryCount(), 5);
  EXPECT_EQ(controller.GetEntryAtIndex(0)->GetURL(), url0);
  EXPECT_EQ(controller.GetEntryAtIndex(1)->GetURL(), url1);
  EXPECT_EQ(controller.GetEntryAtIndex(2)->GetURL(), url2);
  EXPECT_EQ(controller.GetEntryAtIndex(3)->GetURL(), url3_ref);
  EXPECT_EQ(controller.GetEntryAtIndex(4)->GetURL(), url4);
}

// Test that Reload initiates a new navigation to a transient entry's URL.
TEST_F(NavigationControllerTest, ReloadTransient) {
  NavigationControllerImpl& controller = controller_impl();
  const GURL url0("http://foo/0");
  const GURL url1("http://foo/1");
  const GURL transient_url("http://foo/transient");

  // Load |url0|, and start a pending navigation to |url1|.
  controller.LoadURL(
      url0, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  int entry_id = controller.GetPendingEntry()->GetUniqueID();
  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigate(entry_id, true, url0);
  controller.LoadURL(
      url1, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());

  // A transient entry is added, interrupting the navigation.
  std::unique_ptr<NavigationEntry> transient_entry(new NavigationEntryImpl);
  transient_entry->SetURL(transient_url);
  controller.SetTransientEntry(std::move(transient_entry));
  EXPECT_TRUE(controller.GetTransientEntry());
  EXPECT_EQ(transient_url, controller.GetVisibleEntry()->GetURL());

  // The page is reloaded, which should remove the pending entry for |url1| and
  // the transient entry for |transient_url|, and start a navigation to
  // |transient_url|.
  controller.Reload(ReloadType::NORMAL, true);
  entry_id = controller.GetPendingEntry()->GetUniqueID();
  EXPECT_FALSE(controller.GetTransientEntry());
  EXPECT_TRUE(controller.GetPendingEntry());
  EXPECT_EQ(transient_url, controller.GetVisibleEntry()->GetURL());
  ASSERT_EQ(controller.GetEntryCount(), 1);
  EXPECT_EQ(controller.GetEntryAtIndex(0)->GetURL(), url0);

  // Load of |transient_url| completes.
  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigate(entry_id, true, transient_url);
  ASSERT_EQ(controller.GetEntryCount(), 2);
  EXPECT_EQ(controller.GetEntryAtIndex(0)->GetURL(), url0);
  EXPECT_EQ(controller.GetEntryAtIndex(1)->GetURL(), transient_url);
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
  main_test_rfh()->SendRendererInitiatedNavigationRequest(url1, false);
  main_test_rfh()->PrepareForCommit();

  // Simulate what happens if a BrowserURLHandler rewrites the URL, causing
  // the virtual URL to differ from the URL.
  controller.GetPendingEntry()->SetURL(url1_fixed);
  controller.GetPendingEntry()->SetVirtualURL(url1);

  EXPECT_EQ(url1_fixed, controller.GetPendingEntry()->GetURL());
  EXPECT_EQ(url1, controller.GetPendingEntry()->GetVirtualURL());
  EXPECT_TRUE(controller.GetPendingEntry()->is_renderer_initiated());

  // If the user clicks another link, we should replace the pending entry.
  main_test_rfh()->SendRendererInitiatedNavigationRequest(url2, false);
  main_test_rfh()->PrepareForCommit();
  EXPECT_EQ(url2, controller.GetPendingEntry()->GetURL());
  EXPECT_EQ(url2, controller.GetPendingEntry()->GetVirtualURL());

  // Once it commits, the URL and virtual URL should reflect the actual page.
  main_test_rfh()->SendNavigate(0, true, url2);
  EXPECT_EQ(url2, controller.GetLastCommittedEntry()->GetURL());
  EXPECT_EQ(url2, controller.GetLastCommittedEntry()->GetVirtualURL());

  // We should remember if the pending entry will replace the current one.
  // http://crbug.com/308444.
  main_test_rfh()->SendRendererInitiatedNavigationRequest(url2, false);
  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigateWithReplacement(0, false, url2);
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
  controller.LoadURL(
      url0, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  int entry_id = controller.GetPendingEntry()->GetUniqueID();
  EXPECT_EQ(url0, controller.GetPendingEntry()->GetURL());
  EXPECT_EQ(url0, controller.GetVisibleEntry()->GetURL());
  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigate(entry_id, true, url0);

  // For link clicks (renderer-initiated navigations), the pending entry should
  // update before commit but the visible should not.
  NavigationController::LoadURLParams load_url_params(url1);
  load_url_params.is_renderer_initiated = true;
  controller.LoadURLWithParams(load_url_params);
  entry_id = controller.GetPendingEntry()->GetUniqueID();
  EXPECT_EQ(url0, controller.GetVisibleEntry()->GetURL());
  EXPECT_EQ(url1, controller.GetPendingEntry()->GetURL());
  EXPECT_TRUE(controller.GetPendingEntry()->is_renderer_initiated());

  // After commit, both visible should be updated, there should be no pending
  // entry, and we should no longer treat the entry as renderer-initiated.
  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigate(entry_id, true, url1);
  EXPECT_EQ(url1, controller.GetVisibleEntry()->GetURL());
  EXPECT_FALSE(controller.GetPendingEntry());
  EXPECT_FALSE(controller.GetLastCommittedEntry()->is_renderer_initiated());
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
  NavigationController::LoadURLParams load_url_params(url);
  load_url_params.transition_type = ui::PAGE_TRANSITION_LINK;
  load_url_params.is_renderer_initiated = true;
  controller.LoadURLWithParams(load_url_params);
  EXPECT_EQ(url, controller.GetVisibleEntry()->GetURL());
  EXPECT_EQ(url, controller.GetPendingEntry()->GetURL());
  EXPECT_TRUE(controller.GetPendingEntry()->is_renderer_initiated());
  EXPECT_TRUE(controller.IsInitialNavigation());
  EXPECT_FALSE(contents()->HasAccessedInitialDocument());

  // There should be no title yet.
  EXPECT_TRUE(contents()->GetTitle().empty());

  // If something else modifies the contents of the about:blank page, then
  // we must revert to showing about:blank to avoid a URL spoof.
  main_test_rfh()->OnMessageReceived(FrameHostMsg_DidAccessInitialDocument(0));
  EXPECT_TRUE(contents()->HasAccessedInitialDocument());
  EXPECT_FALSE(controller.GetVisibleEntry());
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
  NavigationController::LoadURLParams load_url_params(url);
  load_url_params.transition_type = ui::PAGE_TRANSITION_TYPED;
  load_url_params.is_renderer_initiated = false;
  controller.LoadURLWithParams(load_url_params);
  EXPECT_EQ(url, controller.GetVisibleEntry()->GetURL());
  EXPECT_EQ(url, controller.GetPendingEntry()->GetURL());
  EXPECT_FALSE(controller.GetPendingEntry()->is_renderer_initiated());
  EXPECT_TRUE(controller.IsInitialNavigation());
  EXPECT_FALSE(contents()->HasAccessedInitialDocument());

  // There should be no title yet.
  EXPECT_TRUE(contents()->GetTitle().empty());

  // Suppose it aborts before committing, if it's a 204 or download or due to a
  // stop or a new navigation from the user.  The URL should remain visible.
  static_cast<NavigatorImpl*>(main_test_rfh()->frame_tree_node()->navigator())
      ->CancelNavigation(main_test_rfh()->frame_tree_node(), true);
  EXPECT_EQ(url, controller.GetVisibleEntry()->GetURL());

  // If something else later modifies the contents of the about:blank page, then
  // we must revert to showing about:blank to avoid a URL spoof.
  main_test_rfh()->OnMessageReceived(FrameHostMsg_DidAccessInitialDocument(0));
  EXPECT_TRUE(contents()->HasAccessedInitialDocument());
  EXPECT_FALSE(controller.GetVisibleEntry());
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
  NavigationController::LoadURLParams load_url_params(url);
  load_url_params.transition_type = ui::PAGE_TRANSITION_LINK;
  load_url_params.is_renderer_initiated = true;
  controller.LoadURLWithParams(load_url_params);
  EXPECT_EQ(url, controller.GetVisibleEntry()->GetURL());
  EXPECT_EQ(url, controller.GetPendingEntry()->GetURL());
  EXPECT_TRUE(controller.GetPendingEntry()->is_renderer_initiated());
  EXPECT_TRUE(controller.IsInitialNavigation());
  EXPECT_FALSE(contents()->HasAccessedInitialDocument());

  // There should be no title yet.
  EXPECT_TRUE(contents()->GetTitle().empty());

  // Suppose it aborts before committing, if it's a 204 or download or due to a
  // stop or a new navigation from the user.  The URL should remain visible.
  FrameHostMsg_DidFailProvisionalLoadWithError_Params params;
  params.error_code = net::ERR_ABORTED;
  params.error_description = base::string16();
  params.url = url;
  params.showing_repost_interstitial = false;
  main_test_rfh()->OnMessageReceived(
      FrameHostMsg_DidFailProvisionalLoadWithError(0, params));
  EXPECT_EQ(url, controller.GetVisibleEntry()->GetURL());

  // If something else later modifies the contents of the about:blank page, then
  // we must revert to showing about:blank to avoid a URL spoof.
  main_test_rfh()->OnMessageReceived(FrameHostMsg_DidAccessInitialDocument(0));
  EXPECT_TRUE(contents()->HasAccessedInitialDocument());
  EXPECT_FALSE(controller.GetVisibleEntry());
  EXPECT_EQ(url, controller.GetPendingEntry()->GetURL());
}

TEST_F(NavigationControllerTest, DontShowRendererURLInNewTabAfterCommit) {
  NavigationControllerImpl& controller = controller_impl();

  const GURL url1("http://foo/eh");
  const GURL url2("http://foo/bee");

  // For renderer-initiated navigations in new tabs (with no committed entries),
  // we show the pending entry's URL as long as the about:blank page is not
  // modified.
  NavigationController::LoadURLParams load_url_params(url1);
  load_url_params.transition_type = ui::PAGE_TRANSITION_LINK;
  load_url_params.is_renderer_initiated = true;
  controller.LoadURLWithParams(load_url_params);
  int entry_id = controller.GetPendingEntry()->GetUniqueID();
  EXPECT_EQ(url1, controller.GetVisibleEntry()->GetURL());
  EXPECT_TRUE(controller.GetPendingEntry()->is_renderer_initiated());
  EXPECT_TRUE(controller.IsInitialNavigation());
  EXPECT_FALSE(contents()->HasAccessedInitialDocument());

  // Simulate a commit and then starting a new pending navigation.
  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigate(entry_id, true, url1);
  NavigationController::LoadURLParams load_url2_params(url2);
  load_url2_params.transition_type = ui::PAGE_TRANSITION_LINK;
  load_url2_params.is_renderer_initiated = true;
  controller.LoadURLWithParams(load_url2_params);

  // We should not consider this an initial navigation, and thus should
  // not show the pending URL.
  EXPECT_FALSE(contents()->HasAccessedInitialDocument());
  EXPECT_FALSE(controller.IsInitialNavigation());
  EXPECT_TRUE(controller.GetVisibleEntry());
  EXPECT_EQ(url1, controller.GetVisibleEntry()->GetURL());
}

// Tests that IsURLSameDocumentNavigation returns appropriate results.
// Prevents regression for bug 1126349.
TEST_F(NavigationControllerTest, IsSameDocumentNavigation) {
  NavigationControllerImpl& controller = controller_impl();
  const GURL url("http://www.google.com/home.html");

  // If the renderer claims it performed an same-document navigation from
  // about:blank, trust the renderer.
  // This can happen when an iframe is created and populated via
  // document.write(), then tries to perform a fragment navigation.
  // TODO(japhet): We should only trust the renderer if the about:blank
  // was the first document in the given frame, but we don't have enough
  // information to identify that case currently.
  // TODO(creis): Update this to verify that the origin of the about:blank page
  // matches if the URL doesn't look same-origin.
  const GURL blank_url(url::kAboutBlankURL);
  const url::Origin blank_origin;
  main_test_rfh()->NavigateAndCommitRendererInitiated(true, blank_url);
  EXPECT_TRUE(controller.IsURLSameDocumentNavigation(
      url, url::Origin::Create(url), true, main_test_rfh()));

  // Navigate to URL with no refs.
  NavigationSimulator::NavigateAndCommitFromDocument(url, main_test_rfh());

  // Reloading the page is not a same-document navigation.
  EXPECT_FALSE(controller.IsURLSameDocumentNavigation(
      url, url::Origin::Create(url), false, main_test_rfh()));
  const GURL other_url("http://www.google.com/add.html");
  EXPECT_FALSE(controller.IsURLSameDocumentNavigation(
      other_url, url::Origin::Create(other_url), false, main_test_rfh()));
  const GURL url_with_ref("http://www.google.com/home.html#my_ref");
  EXPECT_TRUE(controller.IsURLSameDocumentNavigation(
      url_with_ref, url::Origin::Create(url_with_ref), true, main_test_rfh()));

  // Navigate to URL with refs.
  NavigationSimulator::NavigateAndCommitFromDocument(url_with_ref,
                                                     main_test_rfh());

  // Reloading the page is not a same-document navigation.
  EXPECT_FALSE(controller.IsURLSameDocumentNavigation(
      url_with_ref, url::Origin::Create(url_with_ref), false, main_test_rfh()));
  EXPECT_FALSE(controller.IsURLSameDocumentNavigation(
      url, url::Origin::Create(url), false, main_test_rfh()));
  EXPECT_FALSE(controller.IsURLSameDocumentNavigation(
      other_url, url::Origin::Create(other_url), false, main_test_rfh()));
  const GURL other_url_with_ref("http://www.google.com/home.html#my_other_ref");
  EXPECT_TRUE(controller.IsURLSameDocumentNavigation(
      other_url_with_ref, url::Origin::Create(other_url_with_ref), true,
      main_test_rfh()));

  // Going to the same url again will be considered same-document navigation
  // if the renderer says it is even if the navigation type isn't SAME_DOCUMENT.
  EXPECT_TRUE(controller.IsURLSameDocumentNavigation(
      url_with_ref, url::Origin::Create(url_with_ref), true, main_test_rfh()));

  // Going back to the non ref url will be considered same-document navigation
  // if the navigation type is SAME_DOCUMENT.
  EXPECT_TRUE(controller.IsURLSameDocumentNavigation(
      url, url::Origin::Create(url), true, main_test_rfh()));

  // If the renderer says this is a same-origin same-document navigation,
  // believe it. This is the pushState/replaceState case.
  EXPECT_TRUE(controller.IsURLSameDocumentNavigation(
      other_url, url::Origin::Create(other_url), true, main_test_rfh()));

  // Don't believe the renderer if it claims a cross-origin navigation is
  // a same-document navigation.
  const GURL different_origin_url("http://www.example.com");
  MockRenderProcessHost* rph = main_test_rfh()->GetProcess();
  EXPECT_EQ(0, rph->bad_msg_count());
  EXPECT_FALSE(controller.IsURLSameDocumentNavigation(
      different_origin_url, url::Origin::Create(different_origin_url), true,
      main_test_rfh()));
  EXPECT_EQ(1, rph->bad_msg_count());
}

// Tests that IsURLSameDocumentNavigation behaves properly with the
// allow_universal_access_from_file_urls flag.
TEST_F(NavigationControllerTest,
       IsSameDocumentNavigationWithUniversalFileAccess) {
  NavigationControllerImpl& controller = controller_impl();

  // Test allow_universal_access_from_file_urls flag.
  const GURL different_origin_url("http://www.example.com");
  MockRenderProcessHost* rph = main_test_rfh()->GetProcess();
  WebPreferences prefs = test_rvh()->GetWebkitPreferences();
  prefs.allow_universal_access_from_file_urls = true;
  test_rvh()->UpdateWebkitPreferences(prefs);
  prefs = test_rvh()->GetWebkitPreferences();
  EXPECT_TRUE(prefs.allow_universal_access_from_file_urls);

  // Allow same-document navigation to be cross-origin if existing URL is file
  // scheme.
  const GURL file_url("file:///foo/index.html");
  const url::Origin file_origin = url::Origin::Create(file_url);
  main_test_rfh()->NavigateAndCommitRendererInitiated(true, file_url);
  EXPECT_TRUE(
      file_origin.IsSameOriginWith(main_test_rfh()->GetLastCommittedOrigin()));
  EXPECT_EQ(0, rph->bad_msg_count());
  EXPECT_TRUE(controller.IsURLSameDocumentNavigation(
      different_origin_url, url::Origin::Create(different_origin_url), true,
      main_test_rfh()));
  EXPECT_EQ(0, rph->bad_msg_count());

  // Doing a replaceState to a cross-origin URL is thus allowed.
  FrameHostMsg_DidCommitProvisionalLoad_Params params;
  params.nav_entry_id = 1;
  params.did_create_new_entry = false;
  params.url = different_origin_url;
  params.origin = file_origin;
  params.transition = ui::PAGE_TRANSITION_LINK;
  params.gesture = NavigationGestureUser;
  params.page_state = PageState::CreateFromURL(different_origin_url);
  params.method = "GET";
  params.post_id = -1;
  main_test_rfh()->SendRendererInitiatedNavigationRequest(different_origin_url,
                                                          false);
  main_test_rfh()->PrepareForCommit();
  contents()->GetMainFrame()->SendNavigateWithParams(&params, true);

  // At this point, we should still consider the current origin to be file://,
  // so that a file URL would still be a same-document navigation.  See
  // https://crbug.com/553418.
  EXPECT_TRUE(
      file_origin.IsSameOriginWith(main_test_rfh()->GetLastCommittedOrigin()));
  EXPECT_TRUE(controller.IsURLSameDocumentNavigation(
      file_url, url::Origin::Create(file_url), true, main_test_rfh()));
  EXPECT_EQ(0, rph->bad_msg_count());

  // Don't honor allow_universal_access_from_file_urls if actual URL is
  // not file scheme.
  const GURL url("http://www.google.com/home.html");
  main_test_rfh()->NavigateAndCommitRendererInitiated(true, url);
  EXPECT_FALSE(controller.IsURLSameDocumentNavigation(
      different_origin_url, url::Origin::Create(different_origin_url), true,
      main_test_rfh()));
  EXPECT_EQ(1, rph->bad_msg_count());
}

// Some pages can have subframes with the same base URL (minus the reference) as
// the main page. Even though this is hard, it can happen, and we don't want
// these subframe navigations to affect the toplevel document. They should
// instead be ignored.  http://crbug.com/5585
TEST_F(NavigationControllerTest, SameSubframe) {
  NavigationControllerImpl& controller = controller_impl();
  // Navigate the main frame.
  const GURL url("http://www.google.com/");
  NavigationSimulator::NavigateAndCommitFromDocument(url, main_test_rfh());

  // We should be at the first navigation entry.
  EXPECT_EQ(controller.GetEntryCount(), 1);
  EXPECT_EQ(controller.GetLastCommittedEntryIndex(), 0);

  // Add and navigate a subframe that would normally count as in-page.
  std::string unique_name("uniqueName0");
  main_test_rfh()->OnCreateChildFrame(
      process()->GetNextRoutingID(),
      TestRenderFrameHost::CreateStubInterfaceProviderRequest(),
      blink::WebTreeScopeType::kDocument, std::string(), unique_name, false,
      base::UnguessableToken::Create(), blink::FramePolicy(),
      FrameOwnerProperties(), blink::FrameOwnerElementType::kIframe);
  TestRenderFrameHost* subframe = static_cast<TestRenderFrameHost*>(
      contents()->GetFrameTree()->root()->child_at(0)->current_frame_host());
  const GURL subframe_url("http://www.google.com/#");
  FrameHostMsg_DidCommitProvisionalLoad_Params params;
  params.nav_entry_id = 0;
  params.did_create_new_entry = false;
  params.url = subframe_url;
  params.transition = ui::PAGE_TRANSITION_AUTO_SUBFRAME;
  params.should_update_history = false;
  params.gesture = NavigationGestureAuto;
  params.method = "GET";
  params.page_state = PageState::CreateFromURL(subframe_url);
  subframe->SendRendererInitiatedNavigationRequest(subframe_url, false);
  subframe->PrepareForCommit();
  subframe->SendNavigateWithParams(&params, false);

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
  const base::string16 title(base::ASCIIToUTF16("Title"));

  NavigateAndCommit(url1);
  controller.GetVisibleEntry()->SetTitle(title);
  NavigateAndCommit(url2);

  std::unique_ptr<WebContents> clone(controller.GetWebContents()->Clone());

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
  const base::string16 title(base::ASCIIToUTF16("Title"));

  NavigateAndCommit(url1);
  controller.GetVisibleEntry()->SetTitle(title);
  NavigateAndCommit(url2);

  std::unique_ptr<WebContents> clone(controller.GetWebContents()->Clone());
  clone->GetController().LoadIfNecessary();

  ASSERT_EQ(2, clone->GetController().GetEntryCount());
  EXPECT_EQ(1, clone->GetController().GetPendingEntryIndex());

  clone->GetController().Reload(ReloadType::NORMAL, true);
  EXPECT_EQ(1, clone->GetController().GetPendingEntryIndex());
}

// Make sure that cloning a WebContentsImpl doesn't copy interstitials.
TEST_F(NavigationControllerTest, CloneOmitsInterstitials) {
  NavigationControllerImpl& controller = controller_impl();
  const GURL url1("http://foo1");
  const GURL url2("http://foo2");

  NavigateAndCommit(url1);
  NavigateAndCommit(url2);

  // Add an interstitial entry.  Should be deleted with controller.
  NavigationEntryImpl* interstitial_entry = new NavigationEntryImpl();
  interstitial_entry->set_page_type(PAGE_TYPE_INTERSTITIAL);
  controller.SetTransientEntry(base::WrapUnique(interstitial_entry));

  std::unique_ptr<WebContents> clone(controller.GetWebContents()->Clone());

  ASSERT_EQ(2, clone->GetController().GetEntryCount());
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

// Test requesting and triggering a lazy reload without any committed entry nor
// pending entry.
TEST_F(NavigationControllerTest, LazyReloadWithoutCommittedEntry) {
  NavigationControllerImpl& controller = controller_impl();
  ASSERT_EQ(-1, controller.GetLastCommittedEntryIndex());
  EXPECT_FALSE(controller.NeedsReload());
  controller.SetNeedsReload();
  EXPECT_TRUE(controller.NeedsReload());

  // Doing a "load if necessary" shouldn't DCHECK.
  controller.LoadIfNecessary();
  ASSERT_FALSE(controller.NeedsReload());
}

// Test requesting and triggering a lazy reload without any committed entry and
// only a pending entry.
TEST_F(NavigationControllerTest, LazyReloadWithOnlyPendingEntry) {
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

// Tests a subframe navigation while a toplevel navigation is pending.
// http://crbug.com/43967
TEST_F(NavigationControllerTest, SubframeWhilePending) {
  NavigationControllerImpl& controller = controller_impl();
  // Load the first page.
  const GURL url1("http://foo/");
  NavigateAndCommit(url1);

  // Now start a pending load to a totally different page, but don't commit it.
  const GURL url2("http://bar/");
  controller.LoadURL(
      url2, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());

  // Send a subframe update from the first page, as if one had just
  // automatically loaded. Auto subframes don't increment the page ID.
  std::string unique_name("uniqueName0");
  main_test_rfh()->OnCreateChildFrame(
      process()->GetNextRoutingID(),
      TestRenderFrameHost::CreateStubInterfaceProviderRequest(),
      blink::WebTreeScopeType::kDocument, std::string(), unique_name, false,
      base::UnguessableToken::Create(), blink::FramePolicy(),
      FrameOwnerProperties(), blink::FrameOwnerElementType::kIframe);
  TestRenderFrameHost* subframe = static_cast<TestRenderFrameHost*>(
      contents()->GetFrameTree()->root()->child_at(0)->current_frame_host());
  const GURL url1_sub("http://foo/subframe");
  FrameHostMsg_DidCommitProvisionalLoad_Params params;
  params.nav_entry_id = 0;
  params.did_create_new_entry = false;
  params.url = url1_sub;
  params.transition = ui::PAGE_TRANSITION_AUTO_SUBFRAME;
  params.should_update_history = false;
  params.gesture = NavigationGestureAuto;
  params.method = "GET";
  params.page_state = PageState::CreateFromURL(url1_sub);

  // This should return false meaning that nothing was actually updated.
  subframe->SendRendererInitiatedNavigationRequest(url1_sub, false);
  subframe->PrepareForCommit();
  subframe->SendNavigateWithParams(&params, false);

  // The notification should have updated the last committed one, and not
  // the pending load.
  EXPECT_EQ(url1, controller.GetLastCommittedEntry()->GetURL());

  // The active entry should be unchanged by the subframe load.
  EXPECT_EQ(url2, controller.GetVisibleEntry()->GetURL());
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
  other_controller.CopyStateFrom(controller, true);

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

// Tests CopyStateFromAndPrune with 2 urls in source, 1 in dest.
TEST_F(NavigationControllerTest, CopyStateFromAndPrune) {
  NavigationControllerImpl& controller = controller_impl();
  const GURL url1("http://foo/1");
  const GURL url2("http://foo/2");
  const GURL url3("http://foo/3");

  NavigateAndCommit(url1);
  NavigateAndCommit(url2);

  // First two entries should have the same SiteInstance.
  SiteInstance* instance1 = controller.GetEntryAtIndex(0)->site_instance();
  SiteInstance* instance2 = controller.GetEntryAtIndex(1)->site_instance();
  EXPECT_EQ(instance1, instance2);

  std::unique_ptr<TestWebContents> other_contents(
      static_cast<TestWebContents*>(CreateTestWebContents().release()));
  NavigationControllerImpl& other_controller = other_contents->GetController();
  other_contents->NavigateAndCommit(url3);
  other_contents->ExpectSetHistoryOffsetAndLength(2, 3);
  other_controller.CopyStateFromAndPrune(&controller, false);

  // other_controller should now contain the 3 urls: url1, url2 and url3.

  ASSERT_EQ(3, other_controller.GetEntryCount());

  ASSERT_EQ(2, other_controller.GetCurrentEntryIndex());

  EXPECT_EQ(url1, other_controller.GetEntryAtIndex(0)->GetURL());
  EXPECT_EQ(url2, other_controller.GetEntryAtIndex(1)->GetURL());
  EXPECT_EQ(url3, other_controller.GetEntryAtIndex(2)->GetURL());

  // A new SiteInstance in a different BrowsingInstance should be used for the
  // new tab.
  SiteInstance* instance3 =
      other_controller.GetEntryAtIndex(2)->site_instance();
  EXPECT_NE(instance3, instance1);
  EXPECT_FALSE(instance3->IsRelatedSiteInstance(instance1));
}

// Test CopyStateFromAndPrune with 2 urls, the first selected and 1 entry in
// the target.
TEST_F(NavigationControllerTest, CopyStateFromAndPrune2) {
  NavigationControllerImpl& controller = controller_impl();
  const GURL url1("http://foo1");
  const GURL url2("http://foo2");
  const GURL url3("http://foo3");

  NavigateAndCommit(url1);
  NavigateAndCommit(url2);
  controller.GoBack();
  contents()->CommitPendingNavigation();

  std::unique_ptr<TestWebContents> other_contents(
      static_cast<TestWebContents*>(CreateTestWebContents().release()));
  NavigationControllerImpl& other_controller = other_contents->GetController();
  other_contents->NavigateAndCommit(url3);
  other_contents->ExpectSetHistoryOffsetAndLength(1, 2);
  other_controller.CopyStateFromAndPrune(&controller, false);

  // other_controller should now contain: url1, url3

  ASSERT_EQ(2, other_controller.GetEntryCount());
  ASSERT_EQ(1, other_controller.GetCurrentEntryIndex());

  EXPECT_EQ(url1, other_controller.GetEntryAtIndex(0)->GetURL());
  EXPECT_EQ(url3, other_controller.GetEntryAtIndex(1)->GetURL());
}

// Test CopyStateFromAndPrune with 2 urls, the last selected and 2 entries in
// the target.
TEST_F(NavigationControllerTest, CopyStateFromAndPrune3) {
  NavigationControllerImpl& controller = controller_impl();
  const GURL url1("http://foo1");
  const GURL url2("http://foo2");
  const GURL url3("http://foo3");
  const GURL url4("http://foo4");

  NavigateAndCommit(url1);
  NavigateAndCommit(url2);

  std::unique_ptr<TestWebContents> other_contents(
      static_cast<TestWebContents*>(CreateTestWebContents().release()));
  NavigationControllerImpl& other_controller = other_contents->GetController();
  other_contents->NavigateAndCommit(url3);
  other_contents->NavigateAndCommit(url4);
  other_contents->ExpectSetHistoryOffsetAndLength(2, 3);
  other_controller.CopyStateFromAndPrune(&controller, false);

  // other_controller should now contain: url1, url2, url4

  ASSERT_EQ(3, other_controller.GetEntryCount());
  ASSERT_EQ(2, other_controller.GetCurrentEntryIndex());

  EXPECT_EQ(url1, other_controller.GetEntryAtIndex(0)->GetURL());
  EXPECT_EQ(url2, other_controller.GetEntryAtIndex(1)->GetURL());
  EXPECT_EQ(url4, other_controller.GetEntryAtIndex(2)->GetURL());
}

// Test CopyStateFromAndPrune with 2 urls, 2 entries in the target, with
// not the last entry selected in the target.
TEST_F(NavigationControllerTest, CopyStateFromAndPruneNotLast) {
  NavigationControllerImpl& controller = controller_impl();
  const GURL url1("http://foo1");
  const GURL url2("http://foo2");
  const GURL url3("http://foo3");
  const GURL url4("http://foo4");

  NavigateAndCommit(url1);
  NavigateAndCommit(url2);

  std::unique_ptr<TestWebContents> other_contents(
      static_cast<TestWebContents*>(CreateTestWebContents().release()));
  NavigationControllerImpl& other_controller = other_contents->GetController();
  other_contents->NavigateAndCommit(url3);
  other_contents->NavigateAndCommit(url4);
  other_controller.GoBack();
  other_contents->CommitPendingNavigation();
  other_contents->ExpectSetHistoryOffsetAndLength(2, 3);
  other_controller.CopyStateFromAndPrune(&controller, false);

  // other_controller should now contain: url1, url2, url3

  ASSERT_EQ(3, other_controller.GetEntryCount());
  ASSERT_EQ(2, other_controller.GetCurrentEntryIndex());

  EXPECT_EQ(url1, other_controller.GetEntryAtIndex(0)->GetURL());
  EXPECT_EQ(url2, other_controller.GetEntryAtIndex(1)->GetURL());
  EXPECT_EQ(url3, other_controller.GetEntryAtIndex(2)->GetURL());
}

// Test CopyStateFromAndPrune with 2 urls, the first selected and 1 entry plus
// a pending entry in the target.
TEST_F(NavigationControllerTest, CopyStateFromAndPruneTargetPending) {
  NavigationControllerImpl& controller = controller_impl();
  const GURL url1("http://foo1");
  const GURL url2("http://foo2");
  const GURL url3("http://foo3");
  const GURL url4("http://foo4");

  NavigateAndCommit(url1);
  NavigateAndCommit(url2);
  controller.GoBack();
  contents()->CommitPendingNavigation();

  std::unique_ptr<TestWebContents> other_contents(
      static_cast<TestWebContents*>(CreateTestWebContents().release()));
  NavigationControllerImpl& other_controller = other_contents->GetController();
  other_contents->NavigateAndCommit(url3);
  other_controller.LoadURL(
      url4, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  other_contents->ExpectSetHistoryOffsetAndLength(1, 2);
  other_controller.CopyStateFromAndPrune(&controller, false);

  // other_controller should now contain url1, url3, and a pending entry
  // for url4.

  ASSERT_EQ(2, other_controller.GetEntryCount());
  EXPECT_EQ(1, other_controller.GetCurrentEntryIndex());

  EXPECT_EQ(url1, other_controller.GetEntryAtIndex(0)->GetURL());
  EXPECT_EQ(url3, other_controller.GetEntryAtIndex(1)->GetURL());

  // And there should be a pending entry for url4.
  ASSERT_TRUE(other_controller.GetPendingEntry());
  EXPECT_EQ(url4, other_controller.GetPendingEntry()->GetURL());
}

// Test CopyStateFromAndPrune with 1 url in the source, 1 entry and a pending
// client redirect entry in the target.  This used to crash
// (http://crbug.com/234809).
TEST_F(NavigationControllerTest, CopyStateFromAndPruneTargetPending2) {
  NavigationControllerImpl& controller = controller_impl();
  const GURL url1("http://foo1");
  const GURL url2a("http://foo2/a");
  const GURL url2b("http://foo2/b");

  NavigateAndCommit(url1);

  std::unique_ptr<TestWebContents> other_contents(
      static_cast<TestWebContents*>(CreateTestWebContents().release()));
  NavigationControllerImpl& other_controller = other_contents->GetController();
  other_contents->NavigateAndCommit(url2a);
  // Simulate a client redirect, which has the same page ID as entry 2a.
  other_controller.LoadURL(
      url2b, Referrer(), ui::PAGE_TRANSITION_LINK, std::string());

  other_contents->ExpectSetHistoryOffsetAndLength(1, 2);
  other_controller.CopyStateFromAndPrune(&controller, false);

  // other_controller should now contain url1, url2a, and a pending entry
  // for url2b.

  ASSERT_EQ(2, other_controller.GetEntryCount());
  EXPECT_EQ(1, other_controller.GetCurrentEntryIndex());

  EXPECT_EQ(url1, other_controller.GetEntryAtIndex(0)->GetURL());
  EXPECT_EQ(url2a, other_controller.GetEntryAtIndex(1)->GetURL());

  // And there should be a pending entry for url4.
  ASSERT_TRUE(other_controller.GetPendingEntry());
  EXPECT_EQ(url2b, other_controller.GetPendingEntry()->GetURL());

  // Let the pending entry commit.
  other_contents->TestDidNavigate(other_contents->GetMainFrame(),
                                  0, false, url2b, ui::PAGE_TRANSITION_LINK);
}

// Test CopyStateFromAndPrune with 2 urls, a back navigation pending in the
// source, and 1 entry in the target. The back pending entry should be ignored.
TEST_F(NavigationControllerTest, CopyStateFromAndPruneSourcePending) {
  NavigationControllerImpl& controller = controller_impl();
  const GURL url1("http://foo1");
  const GURL url2("http://foo2");
  const GURL url3("http://foo3");

  NavigateAndCommit(url1);
  NavigateAndCommit(url2);
  controller.GoBack();

  std::unique_ptr<TestWebContents> other_contents(
      static_cast<TestWebContents*>(CreateTestWebContents().release()));
  NavigationControllerImpl& other_controller = other_contents->GetController();
  other_contents->NavigateAndCommit(url3);
  other_contents->ExpectSetHistoryOffsetAndLength(2, 3);
  other_controller.CopyStateFromAndPrune(&controller, false);

  // other_controller should now contain: url1, url2, url3

  ASSERT_EQ(3, other_controller.GetEntryCount());
  ASSERT_EQ(2, other_controller.GetCurrentEntryIndex());

  EXPECT_EQ(url1, other_controller.GetEntryAtIndex(0)->GetURL());
  EXPECT_EQ(url2, other_controller.GetEntryAtIndex(1)->GetURL());
  EXPECT_EQ(url3, other_controller.GetEntryAtIndex(2)->GetURL());
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
      [](const content::NavigationEntry& entry) { return false; }));
  EXPECT_EQ(0U, navigation_entries_deleted_counter_);
  ASSERT_EQ(5, controller.GetEntryCount());
  ASSERT_EQ(4, controller.GetCurrentEntryIndex());

  // Delete url2 and url4.
  contents()->ExpectSetHistoryOffsetAndLength(2, 3);
  controller.DeleteNavigationEntries(
      base::BindLambdaForTesting([&](const content::NavigationEntry& entry) {
        return entry.GetURL() == url2 || entry.GetURL() == url4;
      }));
  EXPECT_EQ(1U, navigation_entries_deleted_counter_);
  ASSERT_EQ(3, controller.GetEntryCount());
  ASSERT_EQ(2, controller.GetCurrentEntryIndex());
  EXPECT_EQ(url1, controller.GetEntryAtIndex(0)->GetURL());
  EXPECT_EQ(url3, controller.GetEntryAtIndex(1)->GetURL());
  EXPECT_EQ(url5, controller.GetEntryAtIndex(2)->GetURL());
  EXPECT_TRUE(controller.CanGoBack());

  // Delete url1 and url3.
  contents()->ExpectSetHistoryOffsetAndLength(0, 1);
  controller.DeleteNavigationEntries(base::BindRepeating(
      [](const content::NavigationEntry& entry) { return true; }));
  EXPECT_EQ(2U, navigation_entries_deleted_counter_);
  ASSERT_EQ(1, controller.GetEntryCount());
  ASSERT_EQ(0, controller.GetCurrentEntryIndex());
  EXPECT_EQ(url5, controller.GetEntryAtIndex(0)->GetURL());
  EXPECT_FALSE(controller.CanGoBack());

  // No pruned notifications should be send.
  EXPECT_EQ(0U, navigation_list_pruned_counter_);
}

// Tests CopyStateFromAndPrune with 3 urls in source, 1 in dest,
// when the max entry count is 3.  We should prune one entry.
TEST_F(NavigationControllerTest, CopyStateFromAndPruneMaxEntries) {
  NavigationControllerImpl& controller = controller_impl();
  size_t original_count = NavigationControllerImpl::max_entry_count();
  const int kMaxEntryCount = 3;

  NavigationControllerImpl::set_max_entry_count_for_testing(kMaxEntryCount);

  const GURL url1("http://foo/1");
  const GURL url2("http://foo/2");
  const GURL url3("http://foo/3");
  const GURL url4("http://foo/4");


  NavigateAndCommit(url1);
  NavigateAndCommit(url2);
  NavigateAndCommit(url3);

  std::unique_ptr<TestWebContents> other_contents(
      static_cast<TestWebContents*>(CreateTestWebContents().release()));
  NavigationControllerImpl& other_controller = other_contents->GetController();
  other_contents->NavigateAndCommit(url4);
  other_contents->ExpectSetHistoryOffsetAndLength(2, 3);
  other_controller.CopyStateFromAndPrune(&controller, false);

  // We should have received a pruned notification.
  EXPECT_EQ(1U, navigation_list_pruned_counter_);
  EXPECT_TRUE(last_navigation_entry_pruned_details_.from_front);
  EXPECT_EQ(1, last_navigation_entry_pruned_details_.count);

  // other_controller should now contain only 3 urls: url2, url3 and url4.

  ASSERT_EQ(3, other_controller.GetEntryCount());

  ASSERT_EQ(2, other_controller.GetCurrentEntryIndex());

  EXPECT_EQ(url2, other_controller.GetEntryAtIndex(0)->GetURL());
  EXPECT_EQ(url3, other_controller.GetEntryAtIndex(1)->GetURL());
  EXPECT_EQ(url4, other_controller.GetEntryAtIndex(2)->GetURL());

  NavigationControllerImpl::set_max_entry_count_for_testing(original_count);
}

// Tests CopyStateFromAndPrune with 2 urls in source, 1 in dest, with
// replace_entry set to true.
TEST_F(NavigationControllerTest, CopyStateFromAndPruneReplaceEntry) {
  NavigationControllerImpl& controller = controller_impl();
  const GURL url1("http://foo/1");
  const GURL url2("http://foo/2");
  const GURL url3("http://foo/3");

  NavigateAndCommit(url1);
  NavigateAndCommit(url2);

  // First two entries should have the same SiteInstance.
  SiteInstance* instance1 = controller.GetEntryAtIndex(0)->site_instance();
  SiteInstance* instance2 = controller.GetEntryAtIndex(1)->site_instance();
  EXPECT_EQ(instance1, instance2);

  std::unique_ptr<TestWebContents> other_contents(
      static_cast<TestWebContents*>(CreateTestWebContents().release()));
  NavigationControllerImpl& other_controller = other_contents->GetController();
  other_contents->NavigateAndCommit(url3);
  other_contents->ExpectSetHistoryOffsetAndLength(1, 2);
  other_controller.CopyStateFromAndPrune(&controller, true);

  // other_controller should now contain the 2 urls: url1 and url3.

  ASSERT_EQ(2, other_controller.GetEntryCount());

  ASSERT_EQ(1, other_controller.GetCurrentEntryIndex());

  EXPECT_EQ(url1, other_controller.GetEntryAtIndex(0)->GetURL());
  EXPECT_EQ(url3, other_controller.GetEntryAtIndex(1)->GetURL());

  // A new SiteInstance in a different BrowsingInstance should be used for the
  // new tab.
  SiteInstance* instance3 =
      other_controller.GetEntryAtIndex(1)->site_instance();
  EXPECT_NE(instance3, instance1);
  EXPECT_FALSE(instance3->IsRelatedSiteInstance(instance1));
}

// Tests CopyStateFromAndPrune with 3 urls in source, 1 in dest, when the max
// entry count is 3 and replace_entry is true.  We should not prune entries.
TEST_F(NavigationControllerTest, CopyStateFromAndPruneMaxEntriesReplaceEntry) {
  NavigationControllerImpl& controller = controller_impl();
  size_t original_count = NavigationControllerImpl::max_entry_count();
  const int kMaxEntryCount = 3;

  NavigationControllerImpl::set_max_entry_count_for_testing(kMaxEntryCount);

  const GURL url1("http://foo/1");
  const GURL url2("http://foo/2");
  const GURL url3("http://foo/3");
  const GURL url4("http://foo/4");


  NavigateAndCommit(url1);
  NavigateAndCommit(url2);
  NavigateAndCommit(url3);

  std::unique_ptr<TestWebContents> other_contents(
      static_cast<TestWebContents*>(CreateTestWebContents().release()));
  NavigationControllerImpl& other_controller = other_contents->GetController();
  other_contents->NavigateAndCommit(url4);
  other_contents->ExpectSetHistoryOffsetAndLength(2, 3);
  other_controller.CopyStateFromAndPrune(&controller, true);

  // We should have received no pruned notification.
  EXPECT_EQ(0U, navigation_list_pruned_counter_);

  // other_controller should now contain only 3 urls: url1, url2 and url4.

  ASSERT_EQ(3, other_controller.GetEntryCount());

  ASSERT_EQ(2, other_controller.GetCurrentEntryIndex());

  EXPECT_EQ(url1, other_controller.GetEntryAtIndex(0)->GetURL());
  EXPECT_EQ(url2, other_controller.GetEntryAtIndex(1)->GetURL());
  EXPECT_EQ(url4, other_controller.GetEntryAtIndex(2)->GetURL());

  NavigationControllerImpl::set_max_entry_count_for_testing(original_count);
}

// Tests that we can navigate to the restored entries
// imported by CopyStateFromAndPrune.
TEST_F(NavigationControllerTest, CopyRestoredStateAndNavigate) {
  const GURL kRestoredUrls[] = {
    GURL("http://site1.com"),
    GURL("http://site2.com"),
  };
  const GURL kInitialUrl("http://site3.com");

  std::vector<std::unique_ptr<NavigationEntry>> entries;
  for (size_t i = 0; i < arraysize(kRestoredUrls); ++i) {
    std::unique_ptr<NavigationEntry> entry =
        NavigationController::CreateNavigationEntry(
            kRestoredUrls[i], Referrer(), ui::PAGE_TRANSITION_RELOAD, false,
            std::string(), browser_context(),
            nullptr /* blob_url_loader_factory */);
    entries.push_back(std::move(entry));
  }

  // Create a WebContents with restored entries.
  std::unique_ptr<TestWebContents> source_contents(
      static_cast<TestWebContents*>(CreateTestWebContents().release()));
  NavigationControllerImpl& source_controller =
      source_contents->GetController();
  source_controller.Restore(entries.size() - 1,
                            RestoreType::LAST_SESSION_EXITED_CLEANLY, &entries);
  ASSERT_EQ(0u, entries.size());
  source_controller.LoadIfNecessary();
  source_contents->CommitPendingNavigation();

  // Load a page, then copy state from |source_contents|.
  NavigateAndCommit(kInitialUrl);
  contents()->ExpectSetHistoryOffsetAndLength(2, 3);
  controller_impl().CopyStateFromAndPrune(&source_controller, false);
  ASSERT_EQ(3, controller_impl().GetEntryCount());

  // Go back to the first entry one at a time and
  // verify that it works as expected.
  EXPECT_EQ(2, controller_impl().GetCurrentEntryIndex());
  EXPECT_EQ(kInitialUrl, controller_impl().GetLastCommittedEntry()->GetURL());

  controller_impl().GoBack();
  contents()->CommitPendingNavigation();
  EXPECT_EQ(1, controller_impl().GetCurrentEntryIndex());
  EXPECT_EQ(kRestoredUrls[1],
            controller_impl().GetLastCommittedEntry()->GetURL());

  controller_impl().GoBack();
  contents()->CommitPendingNavigation();
  EXPECT_EQ(0, controller_impl().GetCurrentEntryIndex());
  EXPECT_EQ(kRestoredUrls[0],
            controller_impl().GetLastCommittedEntry()->GetURL());
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
  contents()->OnGoToEntryAtOffset(test_rvh(), -1, false);
  EXPECT_EQ(0, controller.GetPendingEntryIndex());

  // Also make sure we told the page to navigate.
  GURL nav_url = GetLastNavigationURL();
  EXPECT_EQ(url1, nav_url);
  contents()->CommitPendingNavigation();
  process()->sink().ClearMessages();

  // Now test history.forward()
  contents()->OnGoToEntryAtOffset(test_rvh(), 2, false);
  EXPECT_EQ(2, controller.GetPendingEntryIndex());

  nav_url = GetLastNavigationURL();
  EXPECT_EQ(url3, nav_url);
  contents()->CommitPendingNavigation();
  process()->sink().ClearMessages();

  controller.DiscardNonCommittedEntries();

  // Make sure an extravagant history.go() doesn't break.
  contents()->OnGoToEntryAtOffset(test_rvh(), 120, false);  // Out of bounds.
  EXPECT_EQ(-1, controller.GetPendingEntryIndex());
  EXPECT_FALSE(HasNavigationRequest());
}

// Test call to PruneAllButLastCommitted for the only entry.
TEST_F(NavigationControllerTest, PruneAllButLastCommittedForSingle) {
  NavigationControllerImpl& controller = controller_impl();
  const GURL url1("http://foo1");
  NavigateAndCommit(url1);

  contents()->ExpectSetHistoryOffsetAndLength(0, 1);

  controller.PruneAllButLastCommitted();

  EXPECT_EQ(-1, controller.GetPendingEntryIndex());
  EXPECT_EQ(controller.GetEntryAtIndex(0)->GetURL(), url1);
}

// Test call to PruneAllButLastCommitted for first entry.
TEST_F(NavigationControllerTest, PruneAllButLastCommittedForFirst) {
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

  contents()->ExpectSetHistoryOffsetAndLength(0, 1);

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

  contents()->ExpectSetHistoryOffsetAndLength(0, 1);

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
  controller.LoadURL(
      url3, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  int entry_id = controller.GetPendingEntry()->GetUniqueID();
  EXPECT_TRUE(controller.GetPendingEntry());
  EXPECT_EQ(2, controller.GetEntryCount());

  contents()->ExpectSetHistoryOffsetAndLength(0, 1);
  controller.PruneAllButLastCommitted();

  // We should only have the last committed and pending entries at this point,
  // and the pending entry should still not be in the entry list.
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(url2, controller.GetEntryAtIndex(0)->GetURL());
  EXPECT_EQ(-1, controller.GetPendingEntryIndex());
  EXPECT_TRUE(controller.GetPendingEntry());
  EXPECT_EQ(1, controller.GetEntryCount());

  // Try to commit the pending entry.
  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigate(entry_id, true, url3);
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
  controller.LoadURL(
      url2, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  EXPECT_FALSE(controller.IsInitialNavigation());
  EXPECT_FALSE(controller.IsInitialBlankNavigation());

  // For cloned tabs, IsInitialNavigationShould be true but
  // IsInitialBlankNavigation should be false.
  std::unique_ptr<WebContents> clone(controller.GetWebContents()->Clone());
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

  main_test_rfh()->SendRendererInitiatedNavigationRequest(kPageWithoutFavicon,
                                                          false);
  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigateWithTransition(
      0,      // nav_entry_id
      false,  // no new entry
      kPageWithoutFavicon, ui::PAGE_TRANSITION_CLIENT_REDIRECT);
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
  main_test_rfh()->SendRendererInitiatedNavigationRequest(kUrl1, false);
  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SendNavigateWithTransition(
      controller.GetEntryAtIndex(0)->GetUniqueID(), false, kUrl1,
      ui::PAGE_TRANSITION_FORWARD_BACK);
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;

  // Verify that the favicon for the page at |kUrl1| was not cleared.
  entry = controller.GetEntryAtIndex(0);
  EXPECT_TRUE(entry);
  EXPECT_EQ(kUrl1, entry->GetURL());
  EXPECT_TRUE(DoImagesMatch(favicon_image, entry->GetFavicon().image));
}

// The test crashes on android: http://crbug.com/170449
#if defined(OS_ANDROID)
#define MAYBE_PurgeScreenshot DISABLED_PurgeScreenshot
#else
#define MAYBE_PurgeScreenshot PurgeScreenshot
#endif
// Tests that screenshot are purged correctly.
TEST_F(NavigationControllerTest, MAYBE_PurgeScreenshot) {
  NavigationControllerImpl& controller = controller_impl();

  NavigationEntryImpl* entry;

  // Navigate enough times to make sure that some screenshots are purged.
  for (int i = 0; i < 12; ++i) {
    const GURL url(base::StringPrintf("http://foo%d/", i));
    NavigateAndCommit(url);
    EXPECT_EQ(i, controller.GetCurrentEntryIndex());
  }

  MockScreenshotManager* screenshot_manager =
      new MockScreenshotManager(&controller);
  controller.SetScreenshotManager(base::WrapUnique(screenshot_manager));
  for (int i = 0; i < controller.GetEntryCount(); ++i) {
    entry = controller.GetEntryAtIndex(i);
    screenshot_manager->TakeScreenshotFor(entry);
    EXPECT_TRUE(entry->screenshot().get());
  }

  NavigateAndCommit(GURL("https://foo/"));
  EXPECT_EQ(13, controller.GetEntryCount());
  entry = controller.GetEntryAtIndex(11);
  screenshot_manager->TakeScreenshotFor(entry);

  for (int i = 0; i < 2; ++i) {
    entry = controller.GetEntryAtIndex(i);
    EXPECT_FALSE(entry->screenshot().get()) << "Screenshot " << i
                                            << " not purged";
  }

  for (int i = 2; i < controller.GetEntryCount() - 1; ++i) {
    entry = controller.GetEntryAtIndex(i);
    EXPECT_TRUE(entry->screenshot().get()) << "Screenshot not found for " << i;
  }

  // Navigate to index 5 and then try to assign screenshot to all entries.
  controller.GoToIndex(5);
  contents()->CommitPendingNavigation();
  EXPECT_EQ(5, controller.GetCurrentEntryIndex());
  for (int i = 0; i < controller.GetEntryCount() - 1; ++i) {
    entry = controller.GetEntryAtIndex(i);
    screenshot_manager->TakeScreenshotFor(entry);
  }

  for (int i = 10; i <= 12; ++i) {
    entry = controller.GetEntryAtIndex(i);
    EXPECT_FALSE(entry->screenshot().get()) << "Screenshot " << i
                                            << " not purged";
    screenshot_manager->TakeScreenshotFor(entry);
  }

  // Navigate to index 7 and assign screenshot to all entries.
  controller.GoToIndex(7);
  contents()->CommitPendingNavigation();
  EXPECT_EQ(7, controller.GetCurrentEntryIndex());
  for (int i = 0; i < controller.GetEntryCount() - 1; ++i) {
    entry = controller.GetEntryAtIndex(i);
    screenshot_manager->TakeScreenshotFor(entry);
  }

  for (int i = 0; i < 2; ++i) {
    entry = controller.GetEntryAtIndex(i);
    EXPECT_FALSE(entry->screenshot().get()) << "Screenshot " << i
                                            << " not purged";
  }

  // Clear all screenshots.
  EXPECT_EQ(13, controller.GetEntryCount());
  EXPECT_EQ(10, screenshot_manager->GetScreenshotCount());
  controller.ClearAllScreenshots();
  EXPECT_EQ(0, screenshot_manager->GetScreenshotCount());
  for (int i = 0; i < controller.GetEntryCount(); ++i) {
    entry = controller.GetEntryAtIndex(i);
    EXPECT_FALSE(entry->screenshot().get()) << "Screenshot " << i
                                            << " not cleared";
  }
}

TEST_F(NavigationControllerTest, PushStateUpdatesTitleAndFavicon) {
  // Navigate.
  NavigationSimulator::NavigateAndCommitFromDocument(GURL("http://foo"),
                                                     main_test_rfh());

  // Set title and favicon.
  base::string16 title(base::ASCIIToUTF16("Title"));
  FaviconStatus favicon;
  favicon.valid = true;
  favicon.url = GURL("http://foo/favicon.ico");
  contents()->UpdateTitleForEntry(
      controller().GetLastCommittedEntry(), title);
  controller().GetLastCommittedEntry()->GetFavicon() = favicon;

  // history.pushState() is called.
  FrameHostMsg_DidCommitProvisionalLoad_Params params;
  GURL kUrl2("http://foo#foo");
  params.nav_entry_id = 0;
  params.did_create_new_entry = true;
  params.url = kUrl2;
  params.page_state = PageState::CreateFromURL(kUrl2);
  main_test_rfh()->SendNavigateWithParams(&params, true);

  // The title should immediately be visible on the new NavigationEntry.
  base::string16 new_title =
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
  controller.GoBack();
  contents()->CommitPendingNavigation();
  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetCurrentEntryIndex());

  // Create a new pending navigation, and indicate that the session history
  // should be cleared.
  NavigationController::LoadURLParams params(url4);
  params.should_clear_history_list = true;
  controller.LoadURLWithParams(params);

  // Verify that the pending entry correctly indicates that the session history
  // should be cleared.
  NavigationEntryImpl* entry = controller.GetPendingEntry();
  ASSERT_TRUE(entry);
  EXPECT_TRUE(entry->should_clear_history_list());

  // Assume that the RenderFrame correctly cleared its history and commit the
  // navigation.
  contents()->GetMainFrame()->SendBeforeUnloadACK(true);
  contents()->GetPendingMainFrame()->
      set_simulate_history_list_was_cleared(true);
  contents()->CommitPendingNavigation();

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

TEST_F(NavigationControllerTest, PostThenReplaceStateThenReload) {
  std::unique_ptr<TestWebContentsDelegate> delegate(
      new TestWebContentsDelegate());
  EXPECT_FALSE(contents()->GetDelegate());
  contents()->SetDelegate(delegate.get());

  // Submit a form.
  GURL url("http://foo");
  FrameHostMsg_DidCommitProvisionalLoad_Params params;
  params.nav_entry_id = 0;
  params.did_create_new_entry = true;
  params.url = url;
  params.transition = ui::PAGE_TRANSITION_FORM_SUBMIT;
  params.gesture = NavigationGestureUser;
  params.page_state = PageState::CreateFromURL(url);
  params.method = "POST";
  params.post_id = 2;
  main_test_rfh()->SendRendererInitiatedNavigationRequest(url, false);
  main_test_rfh()->PrepareForCommit();
  contents()->GetMainFrame()->SendNavigateWithParams(&params, false);

  // history.replaceState() is called.
  GURL replace_url("http://foo#foo");
  params.nav_entry_id = 0;
  params.did_create_new_entry = false;
  params.url = replace_url;
  params.transition = ui::PAGE_TRANSITION_LINK;
  params.gesture = NavigationGestureUser;
  params.page_state = PageState::CreateFromURL(replace_url);
  params.method = "GET";
  params.post_id = -1;
  contents()->GetMainFrame()->SendNavigateWithParams(&params, true);

  // Now reload. replaceState overrides the POST, so we should not show a
  // repost warning dialog.
  controller_impl().Reload(ReloadType::NORMAL, true);
  EXPECT_EQ(0, delegate->repost_form_warning_count());
}

TEST_F(NavigationControllerTest, UnreachableURLGivesErrorPage) {
  GURL url("http://foo");
  controller().LoadURL(url, Referrer(), ui::PAGE_TRANSITION_TYPED,
                       std::string());
  FrameHostMsg_DidCommitProvisionalLoad_Params params;
  params.nav_entry_id = 0;
  params.did_create_new_entry = true;
  params.url = url;
  params.transition = ui::PAGE_TRANSITION_LINK;
  params.gesture = NavigationGestureUser;
  params.page_state = PageState::CreateFromURL(url);
  params.method = "POST";
  params.post_id = 2;
  params.url_is_unreachable = true;

  // Navigate to new page.
  {
    LoadCommittedDetailsObserver observer(contents());
    main_test_rfh()->SendRendererInitiatedNavigationRequest(url, false);
    main_test_rfh()->PrepareForCommit();
    main_test_rfh()->SendNavigateWithParams(&params, false);
    EXPECT_EQ(PAGE_TYPE_ERROR,
              controller_impl().GetLastCommittedEntry()->GetPageType());
    EXPECT_EQ(NAVIGATION_TYPE_NEW_PAGE, observer.navigation_type());
  }

  // Navigate to existing page.
  {
    params.did_create_new_entry = false;
    LoadCommittedDetailsObserver observer(contents());
    main_test_rfh()->SendRendererInitiatedNavigationRequest(url, false);
    main_test_rfh()->PrepareForCommit();
    main_test_rfh()->SendNavigateWithParams(&params, false);
    EXPECT_EQ(PAGE_TYPE_ERROR,
              controller_impl().GetLastCommittedEntry()->GetPageType());
    EXPECT_EQ(NAVIGATION_TYPE_EXISTING_PAGE, observer.navigation_type());
  }

  // Navigate to same page.
  // Note: The call to LoadURL() creates a pending entry in order to trigger the
  // same-page transition.
  controller_impl().LoadURL(
      url, Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  params.nav_entry_id = controller_impl().GetPendingEntry()->GetUniqueID();
  params.transition = ui::PAGE_TRANSITION_TYPED;
  {
    LoadCommittedDetailsObserver observer(contents());
    main_test_rfh()->PrepareForCommit();
    main_test_rfh()->SendNavigateWithParams(&params, false);
    EXPECT_EQ(PAGE_TYPE_ERROR,
              controller_impl().GetLastCommittedEntry()->GetPageType());
    EXPECT_EQ(NAVIGATION_TYPE_SAME_PAGE, observer.navigation_type());
  }

  // Navigate without changing document.
  params.url = GURL("http://foo#foo");
  params.transition = ui::PAGE_TRANSITION_LINK;
  {
    LoadCommittedDetailsObserver observer(contents());
    main_test_rfh()->SendNavigateWithParams(&params, true);
    EXPECT_EQ(PAGE_TYPE_ERROR,
              controller_impl().GetLastCommittedEntry()->GetPageType());
    EXPECT_TRUE(observer.is_same_document());
  }
}

// Tests that if a stale navigation comes back from the renderer, it is properly
// resurrected.
TEST_F(NavigationControllerTest, StaleNavigationsResurrected) {
  NavigationControllerImpl& controller = controller_impl();

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
  controller.GoBack();
  contents()->CommitPendingNavigation();
  EXPECT_EQ(1U, navigation_entry_committed_counter_);
  navigation_entry_committed_counter_ = 0;
  EXPECT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(0, controller.GetCurrentEntryIndex());

  // Start going forward to page B.
  controller.GoForward();

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
  main_test_rfh()->SendNavigate(b_entry_id, false, url_b);
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
// We test the cases where in a navigation is pending/comitted before the new
// navigation is initiated.
// http://crbug.com/664319
TEST_F(NavigationControllerTest, MultipleNavigationsAndReload) {
  NavigationControllerImpl& controller = controller_impl();

  GURL initial_url("http://www.google.com");
  GURL url_1("http://foo.com");
  GURL url_2("http://foo2.com");

  // Test 1.
  // A normal navigation to initial_url should not be marked as a reload.
  controller.LoadURL(initial_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
                     std::string());
  EXPECT_EQ(initial_url, controller.GetVisibleEntry()->GetURL());
  main_test_rfh()->SimulateNavigationStart(initial_url);
  EXPECT_EQ(initial_url, controller.GetVisibleEntry()->GetURL());

  main_test_rfh()->SimulateNavigationCommit(initial_url);
  EXPECT_EQ(ReloadType::NONE, last_reload_type_);

  // Test 2.
  // A navigation to initial_url with the navigation commit delayed should be
  // marked as a reload.
  controller.LoadURL(initial_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
                     std::string());

  EXPECT_EQ(initial_url, controller.GetVisibleEntry()->GetURL());
  main_test_rfh()->SimulateNavigationStart(initial_url);
  EXPECT_EQ(initial_url, controller.GetVisibleEntry()->GetURL());
  EXPECT_EQ(ReloadType::NORMAL, last_reload_type_);

  // Test 3.
  // A navigation to url_1 while the navigation to intial_url is still pending
  // should not be marked as a reload.
  controller.LoadURL(url_1, Referrer(), ui::PAGE_TRANSITION_TYPED,
                     std::string());

  EXPECT_EQ(url_1, controller.GetVisibleEntry()->GetURL());
  main_test_rfh()->SimulateNavigationStart(url_1);
  EXPECT_EQ(url_1, controller.GetVisibleEntry()->GetURL());
  EXPECT_EQ(ReloadType::NONE, last_reload_type_);

  // Test 4.
  // A navigation to url_1 while the previous navigation to url_1 is pending
  // should not be marked as reload. Even though the URL is the same as the
  // previous navigation, the previous navigation did not commit. We can only
  // reload navigations that committed. See https://crbug.com/809040.
  controller.LoadURL(url_1, Referrer(), ui::PAGE_TRANSITION_TYPED,
                     std::string());

  EXPECT_EQ(url_1, controller.GetVisibleEntry()->GetURL());
  main_test_rfh()->SimulateNavigationStart(url_1);
  EXPECT_EQ(url_1, controller.GetVisibleEntry()->GetURL());
  EXPECT_EQ(ReloadType::NONE, last_reload_type_);

  main_test_rfh()->SimulateNavigationCommit(initial_url);

  // Test 5
  // A navigation to url_2 followed by a navigation to the previously pending
  // url_1 should not be marked as a reload.
  controller.LoadURL(url_2, Referrer(), ui::PAGE_TRANSITION_TYPED,
                     std::string());
  EXPECT_EQ(url_2, controller.GetVisibleEntry()->GetURL());
  main_test_rfh()->SimulateNavigationStart(url_2);
  EXPECT_EQ(url_2, controller.GetVisibleEntry()->GetURL());
  EXPECT_EQ(ReloadType::NONE, last_reload_type_);

  controller.LoadURL(url_1, Referrer(), ui::PAGE_TRANSITION_TYPED,
                     std::string());

  EXPECT_EQ(url_1, controller.GetVisibleEntry()->GetURL());
  main_test_rfh()->SimulateNavigationStart(url_1);
  EXPECT_EQ(url_1, controller.GetVisibleEntry()->GetURL());
  EXPECT_EQ(ReloadType::NONE, last_reload_type_);

  main_test_rfh()->SimulateNavigationCommit(url_2);
  main_test_rfh()->SimulateNavigationCommit(url_1);
  main_test_rfh()->SimulateNavigationCommit(url_1);
}

// Test to ensure that the pending entry index is updated when a transient entry
// is inserted or removed.
TEST_F(NavigationControllerTest, PendingEntryIndexUpdatedWithTransient) {
  NavigationControllerImpl& controller = controller_impl();
  const GURL url_0("http://foo/0");
  const GURL url_1("http://foo/1");
  const GURL url_transient_1("http://foo/transient_1");
  const GURL url_transient_2("http://foo/transient_2");

  NavigateAndCommit(url_0);
  NavigateAndCommit(url_1);
  controller.GoBack();
  contents()->CommitPendingNavigation();
  controller.GoForward();

  // Check the state before the insertion of the transient entry.
  // entries[0] = url_0  <- last committed entry.
  // entries[1] = url_1  <- pending entry.
  ASSERT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(1, controller.GetPendingEntryIndex());
  EXPECT_EQ(controller.GetEntryAtIndex(1), controller.GetPendingEntry());
  EXPECT_EQ(url_0, controller.GetEntryAtIndex(0)->GetURL());
  EXPECT_EQ(url_1, controller.GetEntryAtIndex(1)->GetURL());

  // Insert a transient entry before the pending one. It should increase the
  // pending entry index by one (1 -> 2).
  std::unique_ptr<NavigationEntry> transient_entry_1(new NavigationEntryImpl);
  transient_entry_1->SetURL(url_transient_1);
  controller.SetTransientEntry(std::move(transient_entry_1));

  // Check the state after the insertion of the transient entry.
  // entries[0] = url_0           <- last committed entry
  // entries[1] = url_transient_1 <- transient entry
  // entries[2] = url_1           <- pending entry
  ASSERT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(2, controller.GetPendingEntryIndex());
  EXPECT_EQ(controller.GetEntryAtIndex(1), controller.GetTransientEntry());
  EXPECT_EQ(controller.GetEntryAtIndex(2), controller.GetPendingEntry());
  EXPECT_EQ(url_0, controller.GetEntryAtIndex(0)->GetURL());
  EXPECT_EQ(url_transient_1, controller.GetEntryAtIndex(1)->GetURL());
  EXPECT_EQ(url_1, controller.GetEntryAtIndex(2)->GetURL());

  // Insert another transient entry. It should replace the previous one and this
  // time the pending entry index should retain its value (i.e. 2).
  std::unique_ptr<NavigationEntry> transient_entry_2(new NavigationEntryImpl);
  transient_entry_2->SetURL(url_transient_2);
  controller.SetTransientEntry(std::move(transient_entry_2));

  // Check the state after the second insertion of a transient entry.
  // entries[0] = url_0           <- last committed entry
  // entries[1] = url_transient_2 <- transient entry
  // entries[2] = url_1           <- pending entry
  ASSERT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(2, controller.GetPendingEntryIndex());
  EXPECT_EQ(controller.GetEntryAtIndex(1), controller.GetTransientEntry());
  EXPECT_EQ(controller.GetEntryAtIndex(2), controller.GetPendingEntry());
  EXPECT_EQ(url_0, controller.GetEntryAtIndex(0)->GetURL());
  EXPECT_EQ(url_transient_2, controller.GetEntryAtIndex(1)->GetURL());
  EXPECT_EQ(url_1, controller.GetEntryAtIndex(2)->GetURL());

  // Commit the pending entry.
  contents()->CommitPendingNavigation();

  // Check the final state.
  // entries[0] = url_0
  // entries[1] = url_1  <- last committed entry
  ASSERT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(-1, controller.GetPendingEntryIndex());
  EXPECT_EQ(nullptr, controller.GetPendingEntry());
  EXPECT_EQ(nullptr, controller.GetTransientEntry());
  EXPECT_EQ(url_0, controller.GetEntryAtIndex(0)->GetURL());
  EXPECT_EQ(url_1, controller.GetEntryAtIndex(1)->GetURL());
}

// Tests that NavigationUIData has been passed to the NavigationHandle.
TEST_F(NavigationControllerTest, MainFrameNavigationUIData) {
  NavigationControllerImpl& controller = controller_impl();
  LoadCommittedDetailsObserver observer(contents());
  const GURL url1("http://foo1");

  NavigationController::LoadURLParams params(url1);
  params.navigation_ui_data = std::make_unique<TestNavigationUIData>();
  controller.LoadURLWithParams(params);
  int entry_id = controller.GetPendingEntry()->GetUniqueID();

  NavigationRequest* request =
      main_test_rfh()->frame_tree_node()->navigation_request();
  CHECK(request);

  main_test_rfh()->PrepareForCommit();
  main_test_rfh()->SimulateCommitProcessed(
      request->navigation_handle()->GetNavigationId(),
      true /* was_successful */);
  main_test_rfh()->SendNavigate(entry_id, true, url1);

  EXPECT_TRUE(observer.is_main_frame());
  EXPECT_TRUE(observer.has_navigation_ui_data());
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
      TestRenderFrameHost::CreateStubInterfaceProviderRequest(),
      blink::WebTreeScopeType::kDocument, std::string(), unique_name, false,
      base::UnguessableToken::Create(), blink::FramePolicy(),
      FrameOwnerProperties(), blink::FrameOwnerElementType::kIframe);
  TestRenderFrameHost* subframe = static_cast<TestRenderFrameHost*>(
      contents()->GetFrameTree()->root()->child_at(0)->current_frame_host());
  const GURL subframe_url("http://foo1/subframe");

  LoadCommittedDetailsObserver observer(contents());

  // Navigate sub frame.
  NavigationController::LoadURLParams params(url1);
  params.navigation_ui_data = std::make_unique<TestNavigationUIData>();
  params.frame_tree_node_id = subframe->GetFrameTreeNodeId();

#if DCHECK_IS_ON()
  // We DCHECK to prevent misuse of the API.
  EXPECT_DEATH_IF_SUPPORTED(controller_impl().LoadURLWithParams(params), "");
#endif
}

bool SrcDocRewriter(GURL* url, BrowserContext* browser_context) {
  if (*url == GURL("about:srcdoc")) {
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
  BrowserURLHandlerImpl::GetInstance()->SetFixupHandlerForTesting(
      &SrcDocRewriter);

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
      true /* is_renderer_initiated */, main_test_rfh()->GetSiteInstance(),
      Referrer(), ui::PAGE_TRANSITION_LINK,
      false /* should_replace_current_entry */, "GET", nullptr, "", nullptr);

  // Clean up the handler.
  BrowserURLHandlerImpl::GetInstance()->SetFixupHandlerForTesting(nullptr);
}

}  // namespace content
