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
#import "ios/chrome/browser/reader_mode/model/constants.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_content_delegate.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_distiller_viewer.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_eligibility_decider.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_metrics_helper.h"
#import "ios/chrome/browser/translate/model/chrome_ios_translate_client.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

@protocol ReaderModeCommands;

// Observes changes to the web state to perform reader mode operations.
class ReaderModeTabHelper : public web::WebStateObserver,
                            public web::WebStateUserData<ReaderModeTabHelper>,
                            public ReaderModeContentDelegate {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when Reader mode content became available in this tab.
    virtual void ReaderModeWebStateDidLoadContent(
        ReaderModeTabHelper* tab_helper,
        web::WebState* web_state) = 0;
    // Called when Reader mode content will become unavailable in this tab.
    virtual void ReaderModeWebStateWillBecomeUnavailable(
        ReaderModeTabHelper* tab_helper,
        web::WebState* web_state,
        ReaderModeDeactivationReason reason) = 0;

    // Called when distillation fails.
    virtual void ReaderModeDistillationFailed(
        ReaderModeTabHelper* tab_helper) = 0;

    // Called when the ReaderModeTabHelper is destroyed.
    virtual void ReaderModeTabHelperDestroyed(ReaderModeTabHelper* tab_helper,
                                              web::WebState* web_state) = 0;

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
  // mode UI should be presented. GetReaderModeWebState() may still return null.
  bool IsActive() const;
  // Activates Reader mode in the current tab.
  void ActivateReader(ReaderModeAccessPoint access_point);
  // Deactivates Reader mode in the current tab.
  void DeactivateReader(ReaderModeDeactivationReason reason =
                            ReaderModeDeactivationReason::kUserDeactivated);

  // Returns the Reader mode content WebState if it is available. This can be
  // null if Reader mode is active, or non-null while Reader mode is inactive.
  web::WebState* GetReaderModeWebState();

  // Returns whether the current page should be considered for Reader Mode.
  bool CurrentPageIsEligibleForReaderMode() const;
  // Returns whether the current page is distillable.
  bool CurrentPageIsDistillable() const;
  // Returns whether the distillation failed already in the current page
  bool CurrentPageDistillationAlreadyFailed() const;

  // - If the eligibility of the last committed URL is already known, calls
  // `callback` immediately with a boolean value as argument indicating whether
  // the last committed URL is probably distillable.
  // - If the distillability of the last committed URL is not known, waits until
  // the result is available and then calls `callback`.
  // - If the WebState navigates to a different URL (ignoring ref) before the
  // result is available, calls `callback` with nullopt.
  void FetchLastCommittedUrlDistillabilityResult(
      base::OnceCallback<void(std::optional<bool>)> callback);

  // Setter and getter for the readerMode handler.

  void SetReaderModeHandler(id<ReaderModeCommands> reader_mode_handler);

  id<ReaderModeCommands> GetReaderModeHandler() const;

  // Processes the result of the Reader Mode heuristic trigger.
  void HandleReaderModeHeuristicResult(ReaderModeHeuristicResult result);

  // web::WebStateObserver overrides:
  void DidStartNavigation(web::WebState* web_state,
                          web::NavigationContext* navigation_context) override;
  void PageLoaded(
      web::WebState* web_state,
      web::PageLoadCompletionStatus load_completion_status) override;
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  // ReaderModeContentDelegate overrides:
  void ReaderModeContentDidLoadData(
      ReaderModeContentTabHelper* reader_mode_content_tab_helper) override;
  void ReaderModeContentDidCancelRequest(
      ReaderModeContentTabHelper* reader_mode_content_tab_helper,
      NSURLRequest* request,
      web::WebStatePolicyDecider::RequestInfo request_info) override;

  // Sets the script to be used to initialise the scrolling position when the
  // Reader mode content has loaded.
  void SetScrollAnchorScript(std::string script);

  // Returns a weak pointer.
  base::WeakPtr<ReaderModeTabHelper> GetWeakPtr();

 private:
  friend class web::WebStateUserData<ReaderModeTabHelper>;

  // Callback for handling completion of the page distillation.
  void PageDistillationCompleted(
      ReaderModeAccessPoint access_point,
      const GURL& page_url,
      const std::string& html,
      const std::vector<DistillerViewerInterface::ImageInfo>& images,
      const std::string& title,
      const std::string& csp_nonce);

  // Creates `reader_mode_web_state_` if necessary, adds a content tab helper
  // and starts distillation.
  void CreateReaderModeContent(ReaderModeAccessPoint access_point);
  // Destroys the content tab helper in `reader_mode_web_state_` and stops any
  // ongoing distillation.
  void DestroyReaderModeContent(ReaderModeDeactivationReason reason);

  // Cancels any ongoing distillation and destroys the `reader_mode_web_state_`.
  void CancelDistillation();

  // Records the current page distillation failure, when called
  // `distillation_already_failed_` is set to true.
  void RecordDistillationFailure();

  // Completes distillation for the specified `access_point`.
  void CompleteDistillation(ReaderModeAccessPoint access_point);

  // Applies the language settings from the original source page.
  void ApplyLanguageSettingsFromSource();

  // Applies the language settings from the `translate_client`.
  void ApplyLanguageSettingsFromClient(
      ChromeIOSTranslateClient* translate_client);

  // Script to be used to initialise the scrolling position when the Reader mode
  // content has loaded.
  std::string scroll_anchor_script_;

  // Whether Reader mode is active in this tab.
  bool active_ = false;
  // Whether the Reader mode WebState content was loaded.
  bool reader_mode_web_state_content_loaded_ = false;

  // Whether the distillation failed already in the current navigation.
  bool distillation_already_failed_ = false;

  id<ReaderModeCommands> reader_mode_handler_;

  // WebState used to render the Reader mode content. Lazily created the first
  // time Reader mode is activated and persists until the tab is closed.
  std::unique_ptr<web::WebState> reader_mode_web_state_;
  base::OneShotTimer reader_mode_distillation_timer_;

  raw_ptr<web::WebState> web_state_ = nullptr;
  base::ScopedObservation<web::WebState, web::WebStateObserver>
      web_state_observation_{this};
  raw_ptr<DistillerService> distiller_service_;

  std::unique_ptr<ReaderModeDistillerViewer> distiller_viewer_;

  // The state of translation prior to distilling the source page.
  struct TranslationState {
    std::string source_code;
    std::string target_code;
    bool is_original_source_translated = false;
  };
  TranslationState source_translation_state_;

  // Records metrics for the Reader mode with `web_state_`.
  ReaderModeMetricsHelper metrics_helper_;
  ReaderModeEligibilityDecider eligibility_decider_;
  base::ObserverList<Observer, true> observers_;

  base::WeakPtrFactory<ReaderModeTabHelper> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_TAB_HELPER_H_
