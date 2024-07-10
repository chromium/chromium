// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/choose_file/choose_file_controller.h"

#import "base/check.h"
#import "ios/chrome/browser/web/model/choose_file/choose_file_event.h"

ChooseFileController::ChooseFileController(ChooseFileEvent event)
    : choose_file_event_(std::move(event)) {}

ChooseFileController::~ChooseFileController() = default;

const ChooseFileEvent& ChooseFileController::GetChooseFileEvent() const {
  return choose_file_event_;
}

void ChooseFileController::SubmitSelection(NSArray<NSURL*>* file_urls,
                                           NSString* display_string,
                                           UIImage* icon_image) {
  CHECK(!selection_submitted_);
  if (!HasExpired()) {
    DoSubmitSelection(file_urls, display_string, icon_image);
  }
  selection_submitted_ = true;
}

bool ChooseFileController::HasSubmittedSelection() const {
  return selection_submitted_;
}
