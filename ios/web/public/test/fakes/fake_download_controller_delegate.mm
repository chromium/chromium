// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/fakes/fake_download_controller_delegate.h"

#import "base/check.h"
#import "base/check_op.h"
#import "ios/web/public/download/download_controller.h"
#import "ios/web/public/download/download_task.h"

namespace web {

FakeDownloadControllerDelegate::FakeDownloadControllerDelegate(
    DownloadController* controller)
    : controller_(controller) {
  DCHECK(controller_);
  old_delegate_ = controller_->GetDelegate();
  controller_->SetDelegate(this);
}

FakeDownloadControllerDelegate::~FakeDownloadControllerDelegate() {
  controller_->SetDelegate(old_delegate_);
  controller_ = nullptr;
}

void FakeDownloadControllerDelegate::OnDownloadCreated(
    DownloadController* download_controller,
    WebState* web_state,
    std::unique_ptr<DownloadTask> task) {
  alive_download_tasks_.push_back(std::make_pair(web_state, std::move(task)));
}

void FakeDownloadControllerDelegate::OnDownloadControllerDestroyed(
    DownloadController* controller) {
  DCHECK_EQ(controller_, controller);
  controller->SetDelegate(nullptr);
  controller_ = nullptr;
}

}  // namespace web
