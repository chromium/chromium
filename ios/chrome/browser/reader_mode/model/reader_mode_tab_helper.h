// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_TAB_HELPER_H_

#import "base/memory/weak_ptr.h"
#import "base/observer_list.h"
#import "base/scoped_observation.h"
#import "base/timer/timer.h"
#import "ios/chrome/browser/dom_distiller/model/distiller_service.h"
#import "ios/chrome/browser/dom_distiller/model/offline_page_distiller_viewer.h"
#import "ios/chrome/browser/reader_mode/model/constants.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_content_delegate.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

@protocol SnackbarCommands;

// Observes changes to the web state to perform reader mode operations.
class ReaderModeTabHelper : public web::WebStateObserver,
                            public web::WebStateUserData<ReaderModeTabHelper>,
                            public ReaderModeContentDelegate {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when Reader mode content became available in this tab.
    virtual void ReaderModeWebStateDidBecomeAvailable(
        ReaderModeTabHelper* tab_helper) = 0;
    // Called when Reader mode content will become unavailable in this tab.
    virtual void ReaderModeWebStateWillBecomeUnavailable(
        ReaderModeTabHelper* tab_helper) = 0;

    // Called when the ReaderModeTabHelper is destroyed.
    virtual void ReaderModeTabHelperDestroyed(
        ReaderModeTabHelper* tab_helper) = 0;

   protected:
    ~Observer() override = default;
  };

 public:
  ReaderModeTabHelper(web::WebState* web_state,
                      DistillerService* distiller_service);
  ReaderModeTabHelper(const ReaderModeTabHelper&) = delete;
  ReaderModeTabHelper& operator=(const ReaderModeTabHelper&) = delete;

  ~ReaderModeTabHelper() override;

  // Add an observer.
  void AddObserver(Observer* observer);
  // Remove an observer.
  void RemoveObserver(Observer* observer);

  // Returns whether Reader mode is active in the current tab. If so, the Reader
  // mode UI should be presented.
  bool IsActive() const;
  // Activates/deactivates Reader mode in the current tab.
  void SetActive(bool active);

  // Whether the Reader mode WebState is available. When Reader mode becomes
  // active, the Reader mode content will start being generated through
  // distillation. If distillation process is successful, then the Reader mode
  // WebState will become available.
  bool IsReaderModeWebStateAvailable() const;
  // Returns the Reader mode content view. A precondition for calling this
  // method is for `IsReaderModeContentAvailable()` to be true.
  web::WebState* GetReaderModeWebState();

  // Shows the Reader mode options UI.
  void ShowReaderModeOptions();

  // Returns whether the current page supports Reading mode.
  bool CurrentPageSupportsReaderMode() const;
  // - If the eligibility of the last committed URL is already known, calls
  // `callback` immediately with a boolean value as argument indicating whether
  // the last committed URL is eligible.
  // - If the eligibility of the last committed URL is not known, waits until
  // the result is available and then calls `callback`.
  // - If the WebState navigates to a different URL (ignoring ref) before the
  // result is available, calls `callback` with nullopt.
  void FetchLastCommittedUrlEligibilityResult(
      base::OnceCallback<void(std::optional<bool>)> callback);

  // Sets the snackbar handler.
  void SetSnackbarHandler(id<SnackbarCommands> snackbar_handler);

  // Processes the result of the Reader Mode heuristic trigger that was run on
  // the `url` content.
  void HandleReaderModeHeuristicResult(const GURL& url,
                                       ReaderModeHeuristicResult result);

  // Records the Reader Mode heuristic latency from when the JavaScript is
  // executed to when all scores are computed for the heuristic result.
  void RecordReaderModeHeuristicLatency(const base::TimeDelta& latency);

  // web::WebStateObserver overrides:
  void DidStartNavigation(web::WebState* web_state,
                          web::NavigationContext* navigation_context) override;
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void PageLoaded(
      web::WebState* web_state,
      web::PageLoadCompletionStatus load_completion_status) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  // ReaderModeContentDelegate overrides:
  void ReaderModeContentDidCancelRequest(
      ReaderModeContentTabHelper* reader_mode_content_tab_helper,
      NSURLRequest* request,
      web::WebStatePolicyDecider::RequestInfo request_info) override;

 private:
  friend class web::WebStateUserData<ReaderModeTabHelper>;

  // Trigger the heuristic to determine reader mode eligibility.
  void TriggerReaderModeHeuristic(const GURL& url);

  // Starts the reader mode heuristic with a timer.
  void TriggerReaderModeHeuristicAsync(const GURL& url);

  // Resets `reader_mode_eligible_url_` if it is different than the current url
  // context and stops all heuristic triggering.
  void ResetUrlEligibility(const GURL& url);

  // Callback for handling completion of the page distillation.
  void PageDistillationCompleted(
      base::TimeTicks start_time,
      const GURL& page_url,
      const std::string& html,
      const std::vector<DistillerViewerInterface::ImageInfo>& images,
      const std::string& title,
      const std::string& csp_nonce);

  // Creates `reader_mode_web_state_` and starts distillation.
  void CreateReaderModeWebState();
  // Destroys `reader_mode_web_state_` and stops any ongoing distillation.
  void DestroyReaderModeWebState();

  // Sets the last committed URL. If `url` is the equal to the previous value
  // ignoring ref, then this is a no-op.
  void SetLastCommittedUrl(const GURL& url);
  // Calls the callbacks waiting for the last committed URL eligibility result.
  void CallLastCommittedUrlEligibilityCallbacks(std::optional<bool> result);

  // Whether the Reader mode WebState is available in this tab.
  bool reader_mode_web_state_available_ = false;
  // WebState used to render the Reader mode content.
  std::unique_ptr<web::WebState> reader_mode_web_state_;
  id<SnackbarCommands> snackbar_handler_;
  base::TimeDelta heuristic_latency_;
  base::OneShotTimer trigger_reader_mode_timer_;

  // Last committed URL, ignoring ref.
  GURL last_committed_url_without_ref_;
  // Whether the last committed URL eligibility has been determined.
  bool last_committed_url_eligibility_ready_ = false;
  // Callbacks waiting for the last committed URL eligibility result.
  std::vector<base::OnceCallback<void(std::optional<bool>)>>
      last_committed_url_eligibility_callbacks_;

  // Last URL determined eligible to Reader mode in this WebState.
  GURL reader_mode_eligible_url_;
  raw_ptr<web::WebState> web_state_ = nullptr;
  base::ScopedObservation<web::WebState, web::WebStateObserver>
      web_state_observation_{this};
  raw_ptr<DistillerService> distiller_service_;

  std::unique_ptr<OfflinePageDistillerViewer> distiller_viewer_;

  base::ObserverList<Observer, true> observers_;

  base::WeakPtrFactory<ReaderModeTabHelper> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_TAB_HELPER_H_
