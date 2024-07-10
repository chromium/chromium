// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/choose_file/fake_choose_file_controller.h"

#import "ios/chrome/browser/web/model/choose_file/choose_file_event.h"

FakeChooseFileController::FakeChooseFileController(ChooseFileEvent event)
    : ChooseFileController(std::move(event)) {}

FakeChooseFileController::~FakeChooseFileController() = default;

void FakeChooseFileController::SetHasExpired(bool has_expired) {
  has_expired_ = has_expired;
}

void FakeChooseFileController::SetSubmitSelectionCompletion(
    SubmitSelectionCompletion completion) {
  submit_selection_completion_ = std::move(completion);
}

NSArray<NSURL*>* FakeChooseFileController::submitted_file_urls() const {
  return submitted_file_urls_;
}

NSString* FakeChooseFileController::submitted_display_string() const {
  return submitted_display_string_;
}

UIImage* FakeChooseFileController::submitted_icon_image() const {
  return submitted_icon_image_;
}

#pragma mark - ChooseFileController

bool FakeChooseFileController::IsPresentingFilePicker() const {
  return is_presenting_file_picker_;
}

void FakeChooseFileController::SetIsPresentingFilePicker(bool is_presenting) {
  is_presenting_file_picker_ = is_presenting;
}

bool FakeChooseFileController::HasExpired() const {
  return has_expired_;
}

void FakeChooseFileController::DoSubmitSelection(NSArray<NSURL*>* file_urls,
                                                 NSString* display_string,
                                                 UIImage* icon_image) {
  submitted_file_urls_ = file_urls;
  submitted_display_string_ = display_string;
  submitted_icon_image_ = icon_image;
  if (submit_selection_completion_) {
    std::move(submit_selection_completion_).Run(*this);
  }
}
