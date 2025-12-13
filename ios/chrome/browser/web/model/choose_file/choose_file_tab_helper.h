// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_MODEL_CHOOSE_FILE_CHOOSE_FILE_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_WEB_MODEL_CHOOSE_FILE_CHOOSE_FILE_TAB_HELPER_H_

#import <CoreGraphics/CGGeometry.h>

#import "base/scoped_observation.h"
#import "ios/chrome/browser/web/model/choose_file/choose_file_controller.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

class ChooseFileController;
@protocol FileUploadPanelCommands;
@class WKOpenPanelParameters;
@class WKFrameInfo;

class ChooseFileTabHelper : public web::WebStateUserData<ChooseFileTabHelper>,
                            public web::WebStateObserver,
                            public ChooseFileController::Delegate {
 public:
  ~ChooseFileTabHelper() override;

  // Start file selection in the current tab using non-null `controller`.
  void StartChoosingFiles(std::unique_ptr<ChooseFileController> controller);
  // Returns the current ChooseFileController, if any.
  // Returns `nullptr` if `IsChoosingFiles()` is false.
  ChooseFileController* GetChooseFileController();
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

  // Sets the file upload panel handler.
  void SetFileUploadPanelHandler(
      id<FileUploadPanelCommands> file_upload_panel_handler);
  // Displays a file upload panel and calls `completion` with file URLs selected
  // by the user. `parameters` describe the file upload control which initiated
  // the call from `frame`.
  void RunOpenPanel(WKOpenPanelParameters* parameters,
                    WKFrameInfo* frame,
                    base::OnceCallback<void(NSArray<NSURL*>*)> completion)
      API_AVAILABLE(ios(18.4));

  // Set the last ChooseFileEvent.
  void SetLastChooseFileEvent(ChooseFileEvent event);
  // Returns and reset the last ChooseFileEvent.
  std::optional<ChooseFileEvent> ResetLastChooseFileEvent();
  // Returns whether `last_choose_file_event_` has a value.
  bool HasLastChooseFileEvent() const;

  // Adds `file_url` to the set of file URLs ready to be passed to
  // `StopChoosingFiles`. `version_identifier` is used to represent the version
  // of the file which is stored at `file_url`.
  void AddFileUrlReadyForSelection(NSURL* file_url,
                                   NSObject* version_identifier);
  // Removes `file_url` to the set of file URLs ready to be passed to
  // `StopChoosingFiles`.
  void RemoveFileUrlReadyForSelection(NSURL* file_url);
  // Reports true if the following three conditions are verified:
  // 1. `file_url` is contained in `file_urls_ready_for_selection_` ;
  // 2. the identifier associated with `file_url` equals `version_identifier` ;
  // 3. and there is still a file stored at `file_url`.
  // Reports nil otherwise.
  void CheckFileUrlReadyForSelection(
      NSURL* file_url,
      NSObject* version_identifier,
      base::OnceCallback<void(bool)> completion) const;

  // web::WebStateObserver implementation.
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;
  void WasHidden(web::WebState* web_state) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  // ChooseFileController::Delegate implementation.
  void DidSubmitSelection(ChooseFileController* controller,
                          NSArray<NSURL*>* file_urls,
                          NSString* display_string,
                          UIImage* icon_image) override;

 private:
  explicit ChooseFileTabHelper(web::WebState* web_state);
  friend class web::WebStateUserData<ChooseFileTabHelper>;

  // TODO(crbug.com/441659098): Remove once it is not used anywhere.
  template <typename... Args>
  static ChooseFileTabHelper* GetOrCreateForWebState(web::WebState* web_state,
                                                     Args&&... args) {
    CHECK(web_state);
    if (!FromWebState(web_state)) {
      CHECK(!web_state->IsBeingDestroyed());
      web_state->SetUserData(
          UserDataKey(),
          ChooseFileTabHelper::Create(web_state, std::forward<Args>(args)...));
    }

    return FromWebState(web_state);
  }
  friend class FileUploadMenuUpdater;
  friend class DriveFileUploadMenuElement;

  // Abort the current selection flow.
  void AbortSelection();

  // Latest `ChooseFileEvent` received from JavaScript.
  std::optional<ChooseFileEvent> last_choose_file_event_;

  // Handler to show/hide the file upload panel UI.
  __weak id<FileUploadPanelCommands> file_upload_panel_handler_ = nil;

  // URLs of files ready to be submitted using `StopChoosingFiles`. This can be
  // used to reuse a local copy for a file instead of downloading it again. The
  // NSObject associated with a given NSURL can represent the version of a file.
  NSMutableDictionary<NSURL*, NSObject*>* file_urls_ready_for_selection_;

  // When there is a file selection ongoing in the WebState, this controller can
  // be used to keep track of the file selection, submit files, or cancel.
  std::unique_ptr<ChooseFileController> controller_;
  // Scoped observation of the WebState.
  base::ScopedObservation<web::WebState, web::WebStateObserver> observation_{
      this};
};

#endif  // IOS_CHROME_BROWSER_WEB_MODEL_CHOOSE_FILE_CHOOSE_FILE_TAB_HELPER_H_
