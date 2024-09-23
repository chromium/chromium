// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_TEST_NAVIGATION_OBSERVER_H_
#define CONTENT_PUBLIC_TEST_TEST_NAVIGATION_OBSERVER_H_

#include <map>
#include <memory>
#include <optional>

#include "base/callback_list.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/functional/callback.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/test/test_utils.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "third_party/blink/public/common/chrome_debug_urls.h"
#include "third_party/blink/public/mojom/navigation/navigation_initiator_activation_and_ad_status.mojom.h"
#include "url/gurl.h"

namespace content {
class NavigationHandle;
class NavigationRequest;
class WebContents;

// For browser_tests, which run on the UI thread, run a second
// MessageLoop and quit when the navigation completes loading.
class TestNavigationObserver {
 public:
  enum class WaitEvent {
    kLoadStopped,
    kNavigationFinished,
  };

  // Create and register a new TestNavigationObserver against the
  // |web_contents|.
  TestNavigationObserver(WebContents* web_contents,
                         int expected_number_of_navigations,
                         MessageLoopRunner::QuitMode quit_mode =
                             MessageLoopRunner::QuitMode::IMMEDIATE,
                         bool ignore_uncommitted_navigations = true);
  // Like above but waits for one navigation.
  explicit TestNavigationObserver(WebContents* web_contents,
                                  MessageLoopRunner::QuitMode quit_mode =
                                      MessageLoopRunner::QuitMode::IMMEDIATE,
                                  bool ignore_uncommitted_navigations = true);
  // Create and register a new TestNavigationObserver that will wait for
  // a navigation with |target_error|.
  explicit TestNavigationObserver(WebContents* web_contents,
                                  net::Error expected_target_error,
                                  MessageLoopRunner::QuitMode quit_mode =
                                      MessageLoopRunner::QuitMode::IMMEDIATE,
                                  bool ignore_uncommitted_navigations = true);

  // Create and register a new TestNavigationObserver that will wait for
  // |target_url| to complete loading or for a finished navigation to
  // |target_url|.
  explicit TestNavigationObserver(const GURL& expected_target_url,
                                  MessageLoopRunner::QuitMode quit_mode =
                                      MessageLoopRunner::QuitMode::IMMEDIATE,
                                  bool ignore_uncommitted_navigations = true);

  TestNavigationObserver(const TestNavigationObserver&) = delete;
  TestNavigationObserver& operator=(const TestNavigationObserver&) = delete;

  virtual ~TestNavigationObserver();

  void set_expected_initial_url(const GURL& url) {
    // Debug URLs do not go through NavigationRequest and therefore cannot be
    // used as an `expected_initial_url_`.
    DCHECK(!blink::IsRendererDebugURL(url));

    expected_initial_url_ = url;
  }

  void set_wait_event(WaitEvent event) { wait_event_ = event; }

  // Runs a nested run loop and blocks until the expected number of navigations
  // stop loading or |target_url| has loaded.
  void Wait();

  // Runs a nested run loop and blocks until the expected number of navigations
  // finished or a navigation to |target_url| has finished.
  void WaitForNavigationFinished();

  // Start/stop watching newly created WebContents.
  void StartWatchingNewWebContents();
  void StopWatchingNewWebContents();

  // Makes this TestNavigationObserver an observer of all previously created
  // WebContents.
  void WatchExistingWebContents();

  // The URL of the last finished navigation (that matched URL / net error
  // filters, if set).
  const GURL& last_navigation_url() const { return last_navigation_url_; }

  // Returns true if the last finished navigation (that matched URL / net error
  // filters, if set) succeeded.
  bool last_navigation_succeeded() const { return last_navigation_succeeded_; }

  // The last navigation initiator's user activation and ad status.
  blink::mojom::NavigationInitiatorActivationAndAdStatus
  last_navigation_initiator_activation_and_ad_status() const {
    return last_navigation_initiator_activation_and_ad_status_;
  }

  // Returns the initiator origin of the last finished navigation (that matched
  // URL / net error filters, if set).
  const std::optional<url::Origin>& last_initiator_origin() const {
    return last_navigation_initiator_origin_;
  }

  // Returns the frame token of the initiator RenderFrameHost of the last
  // finished navigation. This is defined if and only if
  // last_initiator_process_id below is.
  const std::optional<blink::LocalFrameToken>& last_initiator_frame_token()
      const {
    return last_initiator_frame_token_;
  }

  // Returns the process id of the initiator RenderFrameHost of the last
  // finished navigation. This is defined if and only if
  // last_initiator_frame_token above is, and it is valid only in conjunction
  // with it.
  int last_initiator_process_id() const { return last_initiator_process_id_; }

  // Returns the net::Error origin of the last finished navigation (that matched
  // URL / net error filters, if set).
  net::Error last_net_error_code() const { return last_net_error_code_; }

  // Returns the HTTP response code of the last navigation, if applicable
  std::optional<net::HttpStatusCode> last_http_response_code() const {
    return last_http_response_code_;
  }

  // Returns the navigation entry ID of the last finished navigation (that
  // matched URL if set).
  int last_nav_entry_id() const { return last_nav_entry_id_; }

  SiteInstance* last_source_site_instance() const {
    return last_source_site_instance_.get();
  }

  ukm::SourceId next_page_ukm_source_id() const {
    return next_page_ukm_source_id_;
  }

 protected:
  // Register this TestNavigationObserver as an observer of the |web_contents|.
  void RegisterAsObserver(WebContents* web_contents);

  // Protected so that subclasses can retrieve extra information from the
  // |navigation_handle|.
  virtual void OnDidStartNavigation(NavigationHandle* navigation_handle);

  // Protected so that subclasses can retrieve extra information from the
  // |navigation_handle|.
  virtual void OnDidFinishNavigation(NavigationHandle* navigation_handle);

  // NavigationOfInterestDidFinish is called by OnDidFinishNavigation if it was
  // determined that the finished navigation is on the correct URL, in the
  // correct state, etc. This is the method that classes extending
  // TestNavigationObserver should override, if they wish to intercept data
  // carried in |navigation_handle|.
  virtual void NavigationOfInterestDidFinish(
      NavigationHandle* navigation_handle);

 private:
  class TestWebContentsObserver;

  // State of a WebContents* known to this TestNavigationObserver.
  // Move-only.
  struct WebContentsState {
    WebContentsState();

    WebContentsState(const WebContentsState& other) = delete;
    WebContentsState& operator=(const WebContentsState& other) = delete;
    WebContentsState(WebContentsState&& other);
    WebContentsState& operator=(WebContentsState&& other);

    ~WebContentsState();

    // Observes the WebContents this state has been created for and relays
    // events to the TestNavigationObserver.
    std::unique_ptr<TestWebContentsObserver> observer;

    // If true, a navigation is in progress in the WebContents.
    bool navigation_started = false;
    // If true, the last navigation that finished in this WebContents matched
    // the filter criteria (|target_url_| or |target_error_|).
    // Only relevant if a filter is configured.
    bool last_navigation_matches_filter = false;
  };

  TestNavigationObserver(WebContents* web_contents,
                         int expected_number_of_navigations,
                         const std::optional<GURL>& expected_target_url,
                         std::optional<net::Error> expected_target_error,
                         MessageLoopRunner::QuitMode quit_mode =
                             MessageLoopRunner::QuitMode::IMMEDIATE,
                         bool ignore_uncommitted_navigations = true);

  // Callbacks for WebContents-related events.
  void OnWebContentsCreated(WebContents* web_contents);
  void OnWebContentsDestroyed(TestWebContentsObserver* observer,
                              WebContents* web_contents);
  void OnNavigationEntryCommitted(
      TestWebContentsObserver* observer,
      WebContents* web_contents,
      const LoadCommittedDetails& load_details);
  void OnDidStartLoading(WebContents* web_contents);
  void OnDidStopLoading(WebContents* web_contents);
  void EventTriggered(WebContentsState* web_contents_state);

  // Returns true of |expected_initial_url_| is missing, or if it matches the
  // original URL of the NavigationRequest (stripping the initial view-source:
  // if necessary).
  bool DoesNavigationMatchExpectedInitialUrl(
      NavigationRequest* navigation_request);

  // Returns true if |target_url_| or |target_error_| is configured.
  bool HasFilter();

  // Returns the WebContentsState for |web_contents|.
  WebContentsState* GetWebContentsState(WebContents* web_contents);

  // The event that once triggered will quit the run loop.
  WaitEvent wait_event_;

  // Tracks WebContents and their loading/navigation state.
  std::map<WebContents*, WebContentsState> web_contents_state_;

  // The number of navigations that have been completed.
  int navigations_completed_;

  // The number of navigations to wait for.
  // If |target_url_| and/or |target_error_| are set, only navigations that
  // match those criteria will count towards this.
  int expected_number_of_navigations_;

  // The target URL to wait for.  If this is nullopt, any URL counts.
  const std::optional<GURL> expected_target_url_;

  // The initial URL to wait for.  If this is nullopt, any URL counts.
  std::optional<GURL> expected_initial_url_;

  // The net error of the finished navigation to wait for.
  // If this is nullopt, any net::Error counts.
  const std::optional<net::Error> expected_target_error_;

  // Whether to ignore navigations that finish but don't commit.
  bool ignore_uncommitted_navigations_;

  // The url of the navigation that last committed.
  GURL last_navigation_url_;

  // True if the last navigation succeeded.
  bool last_navigation_succeeded_;

  // The last navigation initiator's user activation and ad status.
  blink::mojom::NavigationInitiatorActivationAndAdStatus
      last_navigation_initiator_activation_and_ad_status_ =
          blink::mojom::NavigationInitiatorActivationAndAdStatus::
              kDidNotStartWithTransientActivation;

  // True if we have called EventTriggered following wait. This is used for
  // internal checks-- we expect certain conditions to be valid until we call
  // EventTriggered at which point we reset state.
  bool was_event_consumed_ = false;

  // The initiator origin of the last navigation.
  std::optional<url::Origin> last_navigation_initiator_origin_;

  // The frame token of the initiator frame for the last observed
  // navigation. This parameter is defined if and only if
  // |initiator_process_id_| below is.
  std::optional<blink::LocalFrameToken> last_initiator_frame_token_;

  // The process id of the initiator frame for the last observed navigation.
  // This is defined if and only if |initiator_frame_token_| above is, and it is
  // only valid in conjunction with it.
  int last_initiator_process_id_ = ChildProcessHost::kInvalidUniqueID;

  // The net error code of the last navigation.
  net::Error last_net_error_code_;

  // HTTP status code of the last navigation.
  std::optional<net::HttpStatusCode> last_http_response_code_ = std::nullopt;

  // The navigation entry ID of the last navigation.
  int last_nav_entry_id_ = 0;

  scoped_refptr<SiteInstance> last_source_site_instance_;

  // The UKM source ID of the next page.
  //
  // For prerender activations, this will retain a bit different UKM source ID
  // from usual. See NavigationHandle::GetNextPageUkmSourceId() for details.
  ukm::SourceId next_page_ukm_source_id_ = ukm::kInvalidSourceId;

  // The MessageLoopRunner used to spin the message loop.
  scoped_refptr<MessageLoopRunner> message_loop_runner_;

  base::CallbackListSubscription creation_subscription_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_TEST_NAVIGATION_OBSERVER_H_
