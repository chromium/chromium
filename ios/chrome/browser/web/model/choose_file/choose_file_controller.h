// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_MODEL_CHOOSE_FILE_CHOOSE_FILE_CONTROLLER_H_
#define IOS_CHROME_BROWSER_WEB_MODEL_CHOOSE_FILE_CHOOSE_FILE_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import <memory>

#import "ios/chrome/browser/web/model/choose_file/choose_file_event.h"

// Controller object to keep track of a file selection flow in a WebState. It is
// created when the user triggers a file input in a web page, and lives until
// the selection is submitted or canceled.
class ChooseFileController {
 public:
  explicit ChooseFileController(ChooseFileEvent event);
  ChooseFileController(const ChooseFileController&) = delete;
  ChooseFileController(ChooseFileController&&) = delete;
  virtual ~ChooseFileController();

  ChooseFileController& operator=(const ChooseFileController&) = delete;
  ChooseFileController& operator=(ChooseFileController&&) = delete;

  // Returns the event associated with this controller.
  const ChooseFileEvent& GetChooseFileEvent() const;

  // Submit file selection. This can only be called once.
  void SubmitSelection(NSArray<NSURL*>* file_urls,
                       NSString* display_string,
                       UIImage* icon_image);
  // Returns whether `SubmitSelection()` has been called.
  bool HasSubmittedSelection() const;

  // Returns whether a file picker is presented.
  virtual bool IsPresentingFilePicker() const = 0;
  // Sets whether a file picker is presented. This should be set to true before
  // presenting a UI which will let the user select files to be submitted.
  virtual void SetIsPresentingFilePicker(bool is_presenting) = 0;

  // Returns whether this controller is expired. If this controller is expired,
  // then calling `SubmitSelection()` is a no-op, and this controller should be
  // destroyed.
  virtual bool HasExpired() const = 0;

 protected:
  // Actually submits the selection.
  virtual void DoSubmitSelection(NSArray<NSURL*>* file_urls,
                                 NSString* display_string,
                                 UIImage* icon_image) = 0;

 private:
  // Set to true when file selection is submitted or canceled.
  bool selection_submitted_ = false;
  // The file selection event associated with this controller.
  ChooseFileEvent choose_file_event_;
};

#endif  // IOS_CHROME_BROWSER_WEB_MODEL_CHOOSE_FILE_CHOOSE_FILE_CONTROLLER_H_
