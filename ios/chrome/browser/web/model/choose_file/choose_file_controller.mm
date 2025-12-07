// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/choose_file/choose_file_controller.h"

#import "base/check.h"
#import "ios/chrome/browser/web/model/choose_file/choose_file_event.h"

ChooseFileController::ChooseFileController(ChooseFileEvent event)
    : choose_file_event_(std::move(event)) {}

ChooseFileController::~ChooseFileController() {
  observers_.Notify(&Observer::ChooseFileControllerDestroyed, this);
}

void ChooseFileController::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
}

void ChooseFileController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ChooseFileController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

const ChooseFileEvent& ChooseFileController::GetChooseFileEvent() const {
  return choose_file_event_;
}

void ChooseFileController::SubmitSelection(NSArray<NSURL*>* file_urls,
                                           NSString* display_string,
                                           UIImage* icon_image) {
  CHECK(!selection_submitted_);
  selection_submitted_ = true;
  if (!HasExpired()) {
    DoSubmitSelection(file_urls, display_string, icon_image);
    if (delegate_) {
      delegate_->DidSubmitSelection(this, file_urls, display_string,
                                    icon_image);
    }
  }
}

bool ChooseFileController::HasSubmittedSelection() const {
  return selection_submitted_;
}

void ChooseFileController::Abort() {
  if (!HasSubmittedSelection() && abort_handler_) {
    std::move(abort_handler_).Run();
  }
}

void ChooseFileController::SetAbortHandler(base::OnceClosure abort_handler) {
  abort_handler_ = std::move(abort_handler);
}
