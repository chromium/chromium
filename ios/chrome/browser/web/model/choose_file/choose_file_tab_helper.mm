// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/choose_file/choose_file_tab_helper.h"

#import "ios/chrome/browser/web/model/choose_file/choose_file_controller.h"
#import "ios/web/public/navigation/navigation_context.h"

ChooseFileTabHelper::ChooseFileTabHelper(web::WebState* web_state) {
  observation_.Observe(web_state);
}

ChooseFileTabHelper::~ChooseFileTabHelper() = default;

void ChooseFileTabHelper::StartChoosingFiles(
    std::unique_ptr<ChooseFileController> controller) {
  CHECK(controller);
  controller_ = std::move(controller);
}

bool ChooseFileTabHelper::IsChoosingFiles() const {
  return controller_ != nullptr;
}

const ChooseFileEvent& ChooseFileTabHelper::GetChooseFileEvent() const {
  CHECK(controller_);
  return controller_->GetChooseFileEvent();
}

bool ChooseFileTabHelper::IsPresentingFilePicker() const {
  CHECK(controller_);
  return controller_->IsPresentingFilePicker();
}

void ChooseFileTabHelper::SetIsPresentingFilePicker(bool is_presenting) {
  CHECK(controller_);
  controller_->SetIsPresentingFilePicker(is_presenting);
}

void ChooseFileTabHelper::StopChoosingFiles(NSArray<NSURL*>* file_urls,
                                            NSString* display_string,
                                            UIImage* icon_image) {
  CHECK(controller_);
  controller_->SubmitSelection(file_urls, display_string, icon_image);
  controller_.reset();
}

#pragma mark - web::WebStateObserver

void ChooseFileTabHelper::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (IsChoosingFiles() && !navigation_context->IsSameDocument()) {
    StopChoosingFiles();
  }
}

void ChooseFileTabHelper::WebStateDestroyed(web::WebState* web_state) {
  observation_.Reset();
}

WEB_STATE_USER_DATA_KEY_IMPL(ChooseFileTabHelper)
