// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_MODEL_VCARD_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_MODEL_VCARD_TAB_HELPER_H_

#import <Foundation/Foundation.h>

#import <set>

#import "base/containers/unique_ptr_adapters.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/scoped_observation.h"
#import "ios/web/public/download/download_task_observer.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

@protocol VcardTabHelperDelegate;
namespace web {
class DownloadTask;
class WebState;
}  // namespace web

// TabHelper which manages Vcard.
class VcardTabHelper : public web::DownloadTaskObserver,
                       public web::WebStateObserver,
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

  // web::WebStateObserver overrides:
  void WasShown(web::WebState* web_state) override;

  // web::DownloadTaskObserver overrides:
  void OnDownloadUpdated(web::DownloadTask* task) override;

  // Called when the downloaded data is available.
  void OnDownloadDataRead(std::unique_ptr<web::DownloadTask> task,
                          NSData* data);

  raw_ptr<web::WebState> web_state_ = nullptr;
  __weak id<VcardTabHelperDelegate> delegate_ = nil;

  // Set of unfinished download tasks.
  std::set<std::unique_ptr<web::DownloadTask>, base::UniquePtrComparator>
      tasks_;

  // The vcard downloaded while the tab was hidden.
  NSData* pending_vcard_ = nil;

  base::ScopedObservation<web::WebState, web::WebStateObserver>
      web_state_observation_{this};

  base::WeakPtrFactory<VcardTabHelper> weak_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_MODEL_VCARD_TAB_HELPER_H_
