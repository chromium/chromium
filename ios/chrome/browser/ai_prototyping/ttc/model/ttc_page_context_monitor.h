// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_PAGE_CONTEXT_MONITOR_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_PAGE_CONTEXT_MONITOR_H_

#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/scoped_observation.h"
#import "base/sequence_checker.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper.h"
#import "ios/web/public/web_state_observer.h"

class GURL;

namespace web {
class NavigationContext;
class WebState;
}  // namespace web

// Monitors a `web::WebState`, notifying `Delegate` when `PageContext`
// (containing `AnnotatedPageContent`) is extracted via `PageContextWrapper`.
class TtcPageContextMonitor : public web::WebStateObserver {
 public:
  // Delegate receiving page context extraction events.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Invoked when page context extraction completes for the monitored tab.
    // `response` contains the extracted `PageContext` proto on success or a
    // `PageContextWrapperError` on failure.
    virtual void OnPageContextFetched(
        PageContextWrapperCallbackResponse response) = 0;
  };

  // Constructs a monitor observing `web_state` and notifying `delegate`.
  // `delegate` must not be `nullptr`. If `web_state` is already realized and
  // finished loading, triggers an initial asynchronous page context extraction
  // immediately.
  TtcPageContextMonitor(web::WebState& web_state, Delegate* delegate);

  TtcPageContextMonitor(const TtcPageContextMonitor&) = delete;
  TtcPageContextMonitor& operator=(const TtcPageContextMonitor&) = delete;

  ~TtcPageContextMonitor() override;

  // Stops observing the monitored `WebState`, cancels any in-flight extraction,
  // and clears the delegate.
  void Disconnect();

  // Returns the monitored `WebState`, or `nullptr` if disconnected or
  // destroyed.
  web::WebState* web_state() const { return web_state_.get(); }

  // `web::WebStateObserver` implementation:
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void PageLoaded(
      web::WebState* web_state,
      web::PageLoadCompletionStatus load_completion_status) override;
  void WebStateDestroyed(web::WebState* web_state) override;

 private:
  friend class TtcPageContextMonitorTest;
  friend class TtcSessionControllerTest;

  // Starts an asynchronous `PageContextWrapper` extraction if `web_state_` is
  // eligible for page context extraction.
  void FetchPageContextIfNeeded();

  // Cancels any in-flight `PageContextWrapper` extraction.
  void CancelPendingExtraction();

  // Invoked when `PageContextWrapper` completes extraction for `expected_url`.
  // Drops `response` if `web_state_` navigated away from `expected_url`.
  void OnPageContextExtracted(const GURL& expected_url,
                              PageContextWrapperCallbackResponse response);

  SEQUENCE_CHECKER(sequence_checker_);

  // Monitored web state. Weak because the `WebState` may be destroyed before
  // the owner destroys this monitor.
  base::WeakPtr<web::WebState> web_state_;

  // Scoped observation of `web_state_`.
  base::ScopedObservation<web::WebState, web::WebStateObserver>
      web_state_observation_{this};

  // Non-owning pointer to the delegate receiving monitor events.
  raw_ptr<Delegate> delegate_ = nullptr;

  // Active wrapper performing asynchronous APC extraction.
  PageContextWrapper* page_context_wrapper_ = nil;

  // Weak pointer factory invalidated whenever a navigation or teardown cancels
  // an in-flight extraction.
  base::WeakPtrFactory<TtcPageContextMonitor> extraction_weak_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_PAGE_CONTEXT_MONITOR_H_
