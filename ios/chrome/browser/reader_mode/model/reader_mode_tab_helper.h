// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_TAB_HELPER_H_

#import "base/memory/weak_ptr.h"
#import "base/timer/timer.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/reader_mode/model/constants.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

@protocol ReaderModeCommands;
@protocol SnackbarCommands;

// Observes changes to the web state to perform reader mode operations.
class ReaderModeTabHelper : public web::WebStateObserver,
                            public web::WebStateUserData<ReaderModeTabHelper> {
 public:
  ReaderModeTabHelper(web::WebState* web_state);
  ReaderModeTabHelper(const ReaderModeTabHelper&) = delete;
  ReaderModeTabHelper& operator=(const ReaderModeTabHelper&) = delete;

  ~ReaderModeTabHelper() override;

  // Returns whether Reader mode is active in the current tab. If so, the Reader
  // mode UI should be presented.
  bool IsActive() const;
  // Activates/deactivates Reader mode in the current tab.
  void SetActive(bool active);
  // Returns the Reader mode content view. A precondition for calling this
  // method is for Reader mode to be active in this tab.
  UIView* GetReaderModeContentView();

  // Sets the snackbar handler.
  void SetSnackbarHandler(id<SnackbarCommands> snackbar_handler);
  // Sets the reader mode handler.
  void SetReaderModeHandler(id<ReaderModeCommands> reader_mode_handler);

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
  void PageLoaded(
      web::WebState* web_state,
      web::PageLoadCompletionStatus load_completion_status) override;
  void WebStateDestroyed(web::WebState* web_state) override;
  void WasHidden(web::WebState* web_state) override;

  // Trigger the heuristic to determine reader mode eligibility.
  void TriggerReaderModeHeuristic();

 private:
  friend class web::WebStateUserData<ReaderModeTabHelper>;

  // Determine if the page load is eligible for triggering the reader mode
  // heuristic.
  bool CanTriggerReaderModeHeuristic();

  // Callback for handling completion of the page distillation.
  void PageDistillationCompleted(ReaderModeHeuristicResult heuristic_result,
                                 base::TimeTicks start_time,
                                 const base::Value* value);

  // Whether Reader mode is active in this tab.
  bool active_ = false;
  // WebState used to render the Reader mode content.
  std::unique_ptr<web::WebState> reader_mode_web_state_;
  id<SnackbarCommands> snackbar_handler_;
  id<ReaderModeCommands> reader_mode_handler_;
  base::TimeDelta heuristic_latency_;
  raw_ptr<web::WebState> web_state_ = nullptr;
  base::OneShotTimer trigger_reader_mode_timer_;
  base::WeakPtrFactory<ReaderModeTabHelper> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_READER_MODE_MODEL_READER_MODE_TAB_HELPER_H_
