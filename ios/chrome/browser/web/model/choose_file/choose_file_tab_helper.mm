// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/choose_file/choose_file_tab_helper.h"

#import <UIKit/UIKit.h>
#import <WebKit/WebKit.h>

#import "base/apple/foundation_util.h"
#import "base/feature_list.h"
#import "base/files/file_util.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/task/thread_pool.h"
#import "ios/chrome/browser/shared/public/commands/file_upload_panel_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/web/model/choose_file/choose_file_controller_impl.h"
#import "ios/chrome/browser/web/model/choose_file/choose_file_event.h"
#import "ios/chrome/browser/web/model/choose_file/choose_file_file_utils.h"
#import "ios/chrome/browser/web/model/choose_file/last_tap_location_tab_helper.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/web_state.h"

ChooseFileTabHelper::ChooseFileTabHelper(web::WebState* web_state)
    : file_urls_ready_for_selection_([NSMutableDictionary dictionary]) {
  observation_.Observe(web_state);
}

ChooseFileTabHelper::~ChooseFileTabHelper() = default;

void ChooseFileTabHelper::StartChoosingFiles(
    std::unique_ptr<ChooseFileController> controller) {
  CHECK(controller);
  controller_ = std::move(controller);
  controller_->SetDelegate(this);
}

ChooseFileController* ChooseFileTabHelper::GetChooseFileController() {
  return controller_.get();
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
}

void ChooseFileTabHelper::SetFileUploadPanelHandler(
    id<FileUploadPanelCommands> file_upload_panel_handler) {
  file_upload_panel_handler_ = file_upload_panel_handler;
}

void ChooseFileTabHelper::RunOpenPanel(
    WKOpenPanelParameters* parameters,
    WKFrameInfo* frame,
    base::OnceCallback<void(NSArray<NSURL*>*)> completion)
    API_AVAILABLE(ios(18.4)) {
  CHECK(base::FeatureList::IsEnabled(kIOSCustomFileUploadMenu));

  web::WebState* web_state = observation_.GetSource();
  if (!web_state || web_state->IsBeingDestroyed() || !web_state->IsVisible()) {
    // If there is no WebState anymore, or it is being destroyed or not shown,
    // then call the completion with no selection and return.
    std::move(completion).Run(nil);
    return;
  }

  std::optional<ChooseFileEvent> last_choose_file_event =
      ResetLastChooseFileEvent();
  base::UmaHistogramBoolean("IOS.Web.FileInput.EventMatched",
                            last_choose_file_event.has_value());
  if (last_choose_file_event.has_value()) {
    if (CGPointEqualToPoint(last_choose_file_event->screen_location,
                            CGPointZero)) {
      last_choose_file_event->screen_location =
          LastTapLocationTabHelper::FromWebState(web_state)->GetLastTapPoint();
    }
    if (!!last_choose_file_event->allow_multiple_files !=
        !!parameters.allowsMultipleSelection) {
      // If the `last_choose_file_event->allow_multiple_files` does not have the
      // correct value according to `parameters`, overwrite it.
      last_choose_file_event->allow_multiple_files =
          parameters.allowsMultipleSelection;
      base::UmaHistogramBoolean("IOS.Web.FileInput.MultipleAttributeMismatched",
                                last_choose_file_event->allow_multiple_files);
    }
    if (!!last_choose_file_event->only_allow_directory !=
        !!parameters.allowsDirectories) {
      // If the `last_choose_file_event->only_allow_directory` does not have the
      // correct value according to `parameters`, overwrite it.
      last_choose_file_event->only_allow_directory =
          parameters.allowsDirectories;
      base::UmaHistogramBoolean(
          "IOS.Web.FileInput.DirectoryAttributeMismatched",
          last_choose_file_event->only_allow_directory);
    }
  } else {
    // If no ChooseFileEvent could be found, create a default event from
    // `parameters`.
    last_choose_file_event =
        ChooseFileEvent::Builder()
            .SetWebState(observation_.GetSource())
            .SetAllowMultipleFiles(parameters.allowsMultipleSelection)
            .SetOnlyAllowDirectory(parameters.allowsDirectories)
            .Build();
  }

  std::unique_ptr<ChooseFileController> choose_file_controller =
      std::make_unique<ChooseFileControllerImpl>(
          std::move(*last_choose_file_event), std::move(completion));
  StartChoosingFiles(std::move(choose_file_controller));

  [file_upload_panel_handler_ showFileUploadPanel];
}

void ChooseFileTabHelper::SetLastChooseFileEvent(ChooseFileEvent event) {
  last_choose_file_event_ = std::move(event);
}

std::optional<ChooseFileEvent> ChooseFileTabHelper::ResetLastChooseFileEvent() {
  return std::exchange(last_choose_file_event_, std::nullopt);
}

bool ChooseFileTabHelper::HasLastChooseFileEvent() const {
  return last_choose_file_event_.has_value();
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

#pragma mark - ChooseFileController::Delegate

void ChooseFileTabHelper::DidSubmitSelection(ChooseFileController* controller,
                                             NSArray<NSURL*>* file_urls,
                                             NSString* display_string,
                                             UIImage* icon_image) {
  CHECK_EQ(controller, controller_.get());
  controller_.reset();
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
