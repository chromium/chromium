// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_MODEL_CHOOSE_FILE_CHOOSE_FILE_CONTROLLER_IMPL_H_
#define IOS_CHROME_BROWSER_WEB_MODEL_CHOOSE_FILE_CHOOSE_FILE_CONTROLLER_IMPL_H_

#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "ios/chrome/browser/web/model/choose_file/choose_file_controller.h"

// Implementation of the abstract class ChooseFileController.
class ChooseFileControllerImpl : public ChooseFileController {
 public:
  ChooseFileControllerImpl(
      ChooseFileEvent event,
      base::OnceCallback<void(NSArray<NSURL*>*)> completion);
  ~ChooseFileControllerImpl() override;

  // ChooseFileController implementation
  bool IsPresentingFilePicker() const override;
  void SetIsPresentingFilePicker(bool is_presenting) override;
  bool HasExpired() const override;
  void DoSubmitSelection(NSArray<NSURL*>* file_urls,
                         NSString* display_string,
                         UIImage* icon_image) override;

 private:
  // Whether a file picker is being presented for this file selection instance.
  bool is_presenting_file_picker_ = false;
  // Whether a file picker has been presented.
  bool file_picker_was_presented_ = false;
  // Completion called with the URLs of files selected by the user.
  base::OnceCallback<void(NSArray<NSURL*>*)> completion_;
};

#endif  // IOS_CHROME_BROWSER_WEB_MODEL_CHOOSE_FILE_CHOOSE_FILE_CONTROLLER_IMPL_H_
