// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/choose_file/choose_file_controller_impl.h"

#import "base/check.h"
#import "ios/chrome/browser/web/model/choose_file/choose_file_event.h"

ChooseFileControllerImpl::ChooseFileControllerImpl(
    ChooseFileEvent event,
    base::OnceCallback<void(NSArray<NSURL*>*)> completion)
    : ChooseFileController(std::move(event)),
      completion_(std::move(completion)) {
  CHECK(completion_);
}

ChooseFileControllerImpl::~ChooseFileControllerImpl() {
  if (file_picker_was_presented_ && !HasSubmittedSelection()) {
    SubmitSelection(nil, nil, nil);
  }
}

bool ChooseFileControllerImpl::IsPresentingFilePicker() const {
  return is_presenting_file_picker_;
}

void ChooseFileControllerImpl::SetIsPresentingFilePicker(bool is_presenting) {
  if (is_presenting) {
    file_picker_was_presented_ = true;
  }
  is_presenting_file_picker_ = is_presenting;
}

bool ChooseFileControllerImpl::HasExpired() const {
  return !completion_;
}

void ChooseFileControllerImpl::DoSubmitSelection(NSArray<NSURL*>* file_urls,
                                                 NSString* display_string,
                                                 UIImage* icon_image) {
  if (completion_) {
    std::move(completion_).Run(file_urls);
  }
}
