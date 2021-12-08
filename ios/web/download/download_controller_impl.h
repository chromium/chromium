// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_DOWNLOAD_DOWNLOAD_CONTROLLER_IMPL_H_
#define IOS_WEB_DOWNLOAD_DOWNLOAD_CONTROLLER_IMPL_H_

#include <Foundation/Foundation.h>

#include <set>

#include "base/sequence_checker.h"
#include "base/supports_user_data.h"
#import "ios/web/download/download_task_impl.h"
#import "ios/web/public/download/download_controller.h"
#include "ui/base/page_transition_types.h"

namespace web {

class DownloadControllerDelegate;
class WebState;

class DownloadControllerImpl : public DownloadController,
                               public base::SupportsUserData::Data,
                               public DownloadTaskImpl::Delegate {
 public:
  DownloadControllerImpl();

  DownloadControllerImpl(const DownloadControllerImpl&) = delete;
  DownloadControllerImpl& operator=(const DownloadControllerImpl&) = delete;

  ~DownloadControllerImpl() override;

  // DownloadController overrides:
  void CreateDownloadTask(WebState* web_state,
                          NSString* identifier,
                          const GURL& original_url,
                          NSString* http_method,
                          const std::string& content_disposition,
                          int64_t total_bytes,
                          const std::string& mime_type) override;

  void CreateNativeDownloadTask(WebState* web_state,
                                NSString* identifier,
                                const GURL& original_url,
                                NSString* http_method,
                                const std::string& content_disposition,
                                int64_t total_bytes,
                                const std::string& mime_type,
                                DownloadNativeTaskBridge* download) override
      API_AVAILABLE(ios(15));

  void SetDelegate(DownloadControllerDelegate* delegate) override;
  DownloadControllerDelegate* GetDelegate() const override;

  // DownloadTaskImpl::Delegate overrides:
  void OnTaskDestroyed(DownloadTaskImpl* task) override;
  NSURLSession* CreateSession(NSString* identifier,
                              NSArray<NSHTTPCookie*>* cookies,
                              id<NSURLSessionDataDelegate> delegate,
                              NSOperationQueue* delegate_queue) override;

 private:
  // Set of tasks which are currently alive.
  std::set<DownloadTaskImpl*> alive_tasks_;
  DownloadControllerDelegate* delegate_ = nullptr;
  SEQUENCE_CHECKER(my_sequence_checker_);
};

}  // namespace web

#endif  // IOS_WEB_DOWNLOAD_DOWNLOAD_CONTROLLER_IMPL_H_
