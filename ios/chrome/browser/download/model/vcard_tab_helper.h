// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_MODEL_VCARD_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_MODEL_VCARD_TAB_HELPER_H_

#import <Foundation/Foundation.h>

#include <set>

#include "base/containers/unique_ptr_adapters.h"
#include "base/memory/weak_ptr.h"
#include "ios/web/public/download/download_task_observer.h"
#include "ios/web/public/web_state_user_data.h"

@protocol VcardTabHelperDelegate;
namespace web {
class DownloadTask;
class WebState;
}  // namespace web

// TabHelper which manages Vcard.
class VcardTabHelper : public web::DownloadTaskObserver,
                       public web::WebStateUserData<VcardTabHelper> {
 public:
  VcardTabHelper(const VcardTabHelper&) = delete;
  VcardTabHelper& operator=(const VcardTabHelper&) = delete;

  ~VcardTabHelper() override;

  id<VcardTabHelperDelegate> delegate() { return delegate_; }

  // `delegate` is not retained by this instance.
  void set_delegate(id<VcardTabHelperDelegate> delegate) {
    delegate_ = delegate;
  }

  // Asynchronously downloads the Vcard file using the given `task`. Asks
  // delegate to open the Vcard when the download is complete.
  virtual void Download(std::unique_ptr<web::DownloadTask> task);

 protected:
  // Allow subclassing from VcardTabHelper for testing purposes.
  explicit VcardTabHelper(web::WebState* web_state);

 private:
  friend class web::WebStateUserData<VcardTabHelper>;

  // web::DownloadTaskObserver overrides:
  void OnDownloadUpdated(web::DownloadTask* task) override;

  // Called when the downloaded data is available.
  void OnDownloadDataRead(std::unique_ptr<web::DownloadTask> task,
                          NSData* data);

  __weak id<VcardTabHelperDelegate> delegate_ = nil;

  // Set of unfinished download tasks.
  std::set<std::unique_ptr<web::DownloadTask>, base::UniquePtrComparator>
      tasks_;

  base::WeakPtrFactory<VcardTabHelper> weak_factory_{this};

  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_MODEL_VCARD_TAB_HELPER_H_
