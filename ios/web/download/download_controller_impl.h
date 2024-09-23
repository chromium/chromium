// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_DOWNLOAD_DOWNLOAD_CONTROLLER_IMPL_H_
#define IOS_WEB_DOWNLOAD_DOWNLOAD_CONTROLLER_IMPL_H_

#include <Foundation/Foundation.h>

#include <set>

#import "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/supports_user_data.h"
#include "ios/web/download/download_task_impl.h"
#include "ios/web/public/download/download_controller.h"
#include "ios/web/public/download/download_task_observer.h"
#include "ui/base/page_transition_types.h"

namespace base {
class SequencedTaskRunner;
}

namespace web {

class DownloadControllerDelegate;
class WebState;

class DownloadControllerImpl : public DownloadController,
                               public base::SupportsUserData::Data,
                               public DownloadTaskObserver {
 public:
  DownloadControllerImpl();

  DownloadControllerImpl(const DownloadControllerImpl&) = delete;
  DownloadControllerImpl& operator=(const DownloadControllerImpl&) = delete;

  ~DownloadControllerImpl() override;

  // DownloadController overrides:
  void CreateNativeDownloadTask(WebState* web_state,
                                NSString* identifier,
                                const GURL& original_url,
                                NSString* http_method,
                                const std::string& content_disposition,
                                int64_t total_bytes,
                                const std::string& mime_type,
                                DownloadNativeTaskBridge* download) override;

  void CreateWebStateDownloadTask(WebState* web_state,
                                  NSString* identifier,
                                  int64_t total_bytes) override;

  void SetDelegate(DownloadControllerDelegate* delegate) override;
  DownloadControllerDelegate* GetDelegate() const override;

  // DownloadTaskObserver overrides:
  void OnDownloadDestroyed(DownloadTask* task) override;

 private:
  // Called when a new task is created.
  void OnDownloadCreated(std::unique_ptr<DownloadTaskImpl> task);

  // Set of tasks which are currently alive.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  std::set<DownloadTask*> alive_tasks_;
  raw_ptr<DownloadControllerDelegate> delegate_ = nullptr;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace web

#endif  // IOS_WEB_DOWNLOAD_DOWNLOAD_CONTROLLER_IMPL_H_
