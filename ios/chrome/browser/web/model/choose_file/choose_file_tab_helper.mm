// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/choose_file/choose_file_tab_helper.h"

#import "base/apple/foundation_util.h"
#import "base/files/file_util.h"
#import "base/task/thread_pool.h"
#import "ios/chrome/browser/web/model/choose_file/choose_file_controller.h"
#import "ios/chrome/browser/web/model/choose_file/choose_file_file_utils.h"
#import "ios/web/public/navigation/navigation_context.h"

ChooseFileTabHelper::ChooseFileTabHelper(web::WebState* web_state)
    : file_urls_ready_for_selection_([NSMutableDictionary dictionary]) {
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
  CHECK([[NSSet setWithArray:file_urls]
      isSubsetOfSet:[NSSet setWithArray:[file_urls_ready_for_selection_
                                            allKeys]]]);
  controller_->SubmitSelection(file_urls, display_string, icon_image);
  controller_.reset();
}

void ChooseFileTabHelper::AbortSelection() {
  if (IsChoosingFiles()) {
    controller_->Abort();
    controller_.reset();
  }
}

void ChooseFileTabHelper::AddFileUrlReadyForSelection(
    NSURL* file_url,
    NSObject* version_identifier) {
  if (!version_identifier) {
    // If `version_identifier` is nil, set it to NSNull to indicate that some
    // version of the file is ready for selection.
    version_identifier = [NSNull null];
  }
  file_urls_ready_for_selection_[file_url] = version_identifier;
}

void ChooseFileTabHelper::RemoveFileUrlReadyForSelection(NSURL* file_url) {
  CHECK(file_url);
  [file_urls_ready_for_selection_ removeObjectForKey:file_url];
}

void ChooseFileTabHelper::CheckFileUrlReadyForSelection(
    NSURL* file_url,
    NSObject* version_identifier,
    base::OnceCallback<void(bool)> completion) const {
  if (!version_identifier) {
    // If `version_identifier` is nil, set it to NSNull to indicate that any
    // version of the file needs to be ready for selection.
    version_identifier = [NSNull null];
  }
  NSObject* expected_version_identifier =
      [file_urls_ready_for_selection_ objectForKey:file_url];
  if (!expected_version_identifier ||
      ![expected_version_identifier isEqual:version_identifier]) {
    std::move(completion).Run(false);
    return;
  }
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(base::PathExists, base::apple::NSURLToFilePath(file_url)),
      std::move(completion));
}

#pragma mark - web::WebStateObserver

void ChooseFileTabHelper::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (!navigation_context->IsSameDocument()) {
    AbortSelection();
  }
}

void ChooseFileTabHelper::WasHidden(web::WebState* web_state) {
  AbortSelection();
}

void ChooseFileTabHelper::WebStateDestroyed(web::WebState* web_state) {
  AbortSelection();
  DeleteTempChooseFileDirectoryForTab(web_state->GetUniqueIdentifier());
  observation_.Reset();
}

WEB_STATE_USER_DATA_KEY_IMPL(ChooseFileTabHelper)
