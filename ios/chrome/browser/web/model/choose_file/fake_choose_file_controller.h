// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_MODEL_CHOOSE_FILE_FAKE_CHOOSE_FILE_CONTROLLER_H_
#define IOS_CHROME_BROWSER_WEB_MODEL_CHOOSE_FILE_FAKE_CHOOSE_FILE_CONTROLLER_H_

#import "base/functional/callback.h"
#import "ios/chrome/browser/web/model/choose_file/choose_file_controller.h"

// Fake implementation of ChooseFileController for testing purposes.
class FakeChooseFileController : public ChooseFileController {
 public:
  explicit FakeChooseFileController(ChooseFileEvent event);
  ~FakeChooseFileController() override;

  // Sets the value returned by `HasExpired()`.
  void SetHasExpired(bool has_expired);
  // Sets the callback called as completion of `DoSubmitSelection()`.
  using SubmitSelectionCompletion =
      base::OnceCallback<void(const FakeChooseFileController&)>;
  void SetSubmitSelectionCompletion(SubmitSelectionCompletion completion);

  // Return the arguments passed to `DoSubmitSelection()`.
  NSArray<NSURL*>* submitted_file_urls() const;
  NSString* submitted_display_string() const;
  UIImage* submitted_icon_image() const;

  // ChooseFileController implementation.
  bool IsPresentingFilePicker() const override;
  void SetIsPresentingFilePicker(bool is_presenting) override;
  bool HasExpired() const override;
  void DoSubmitSelection(NSArray<NSURL*>* file_urls,
                         NSString* display_string,
                         UIImage* icon_image) override;

 private:
  bool is_presenting_file_picker_ = false;
  bool has_expired_ = false;
  NSArray<NSURL*>* submitted_file_urls_ = nil;
  NSString* submitted_display_string_ = nil;
  UIImage* submitted_icon_image_ = nil;
  SubmitSelectionCompletion submit_selection_completion_;
};

#endif  // IOS_CHROME_BROWSER_WEB_MODEL_CHOOSE_FILE_FAKE_CHOOSE_FILE_CONTROLLER_H_
