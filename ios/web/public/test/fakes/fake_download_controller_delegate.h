// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_FAKES_FAKE_DOWNLOAD_CONTROLLER_DELEGATE_H_
#define IOS_WEB_PUBLIC_TEST_FAKES_FAKE_DOWNLOAD_CONTROLLER_DELEGATE_H_

#include <memory>
#include <vector>

#import "base/memory/raw_ptr.h"
#include "ios/web/public/download/download_controller_delegate.h"

namespace web {

class DownloadController;
class DownloadTask;
class WebState;

// DownloadControllerDelegate which captures tasks passed to OnDownloadCreated.
class FakeDownloadControllerDelegate : public DownloadControllerDelegate {
 public:
  FakeDownloadControllerDelegate(DownloadController* controller);

  FakeDownloadControllerDelegate(const FakeDownloadControllerDelegate&) =
      delete;
  FakeDownloadControllerDelegate& operator=(
      const FakeDownloadControllerDelegate&) = delete;

  ~FakeDownloadControllerDelegate() override;

  using AliveDownloadTaskList =
      std::vector<std::pair<const WebState*, std::unique_ptr<DownloadTask>>>;
  // Returns downloads created via OnDownloadCreated and not yet destroyed.
  const AliveDownloadTaskList& alive_download_tasks() const {
    return alive_download_tasks_;
  }

 private:
  // DownloadControllerDelegate overrides:
  void OnDownloadCreated(DownloadController*,
                         WebState*,
                         std::unique_ptr<DownloadTask>) override;
  void OnDownloadControllerDestroyed(DownloadController*) override;

  raw_ptr<DownloadControllerDelegate> old_delegate_ = nullptr;
  raw_ptr<DownloadController> controller_ = nullptr;
  AliveDownloadTaskList alive_download_tasks_;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_FAKES_FAKE_DOWNLOAD_CONTROLLER_DELEGATE_H_
