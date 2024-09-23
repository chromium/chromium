// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_MODEL_CHOOSE_FILE_CHOOSE_FILE_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_WEB_MODEL_CHOOSE_FILE_CHOOSE_FILE_TAB_HELPER_H_

#import "base/scoped_observation.h"
#import "ios/chrome/browser/web/model/choose_file/choose_file_controller.h"
#import "ios/web/public/lazy_web_state_user_data.h"
#import "ios/web/public/web_state_observer.h"

class ChooseFileController;

class ChooseFileTabHelper
    : public web::LazyWebStateUserData<ChooseFileTabHelper>,
      public web::WebStateObserver {
 public:
  ~ChooseFileTabHelper() override;

  // Start file selection in the current tab using non-null `controller`.
  void StartChoosingFiles(std::unique_ptr<ChooseFileController> controller);
  // Returns whether a file selection is ongoing in the current tab.
  bool IsChoosingFiles() const;
  // Returns the event associated with the current file selection.
  const ChooseFileEvent& GetChooseFileEvent() const;
  // Returns whether a file picker is presented.
  // Can only be called if `IsChoosingFiles()` is true.
  bool IsPresentingFilePicker() const;
  // Sets whether a file picker is presented. This should be set to true before
  // presenting a UI which will let the user select files to be submitted.
  // Can only be called if `IsChoosingFiles()` is true.
  void SetIsPresentingFilePicker(bool is_presenting);
  // Submits file selection. Can only be called if `IsChoosingFiles()` is true.
  void StopChoosingFiles(NSArray<NSURL*>* file_urls = @[],
                         NSString* display_string = nil,
                         UIImage* icon_image = nil);

  // web::WebStateObserver implementation.
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void WebStateDestroyed(web::WebState* web_state) override;

 private:
  explicit ChooseFileTabHelper(web::WebState* web_state);
  friend class web::LazyWebStateUserData<ChooseFileTabHelper>;
  WEB_STATE_USER_DATA_KEY_DECL();

  // When there is a file selection ongoing in the WebState, this controller can
  // be used to keep track of the file selection, submit files, or cancel.
  std::unique_ptr<ChooseFileController> controller_;
  // Scoped observation of the WebState.
  base::ScopedObservation<web::WebState, web::WebStateObserver> observation_{
      this};
};

#endif  // IOS_CHROME_BROWSER_WEB_MODEL_CHOOSE_FILE_CHOOSE_FILE_TAB_HELPER_H_
