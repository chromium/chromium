// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/test/fakes/fake_download_controller_delegate.h"

#include "ios/web/public/download/download_controller.h"
#include "ios/web/public/download/download_task.h"

namespace web {

FakeDownloadControllerDelegate::FakeDownloadControllerDelegate(
    DownloadController* controller) {
  controller->SetDelegate(this);
}

FakeDownloadControllerDelegate::~FakeDownloadControllerDelegate() = default;

void FakeDownloadControllerDelegate::OnDownloadCreated(
    DownloadController* download_controller,
    WebState* web_state,
    std::unique_ptr<DownloadTask> task) {
  alive_download_tasks_.push_back(std::make_pair(web_state, std::move(task)));
}

void FakeDownloadControllerDelegate::OnDownloadControllerDestroyed(
    DownloadController* controller) {
  controller->SetDelegate(nullptr);
}

}  // namespace web
