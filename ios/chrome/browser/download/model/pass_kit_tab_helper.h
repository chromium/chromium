// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_MODEL_PASS_KIT_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_MODEL_PASS_KIT_TAB_HELPER_H_

#import <Foundation/Foundation.h>

#include <memory>
#include <set>

#include "base/containers/unique_ptr_adapters.h"
#import "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ios/web/public/download/download_task_observer.h"
#include "ios/web/public/lazy_web_state_user_data.h"

@class JSUnzipper;
@protocol WebContentCommands;
namespace web {
class DownloadTask;
class WebState;
}  // namespace web

// Key of the UMA Download.IOSDownloadPassKitResult histogram.
extern const char kUmaDownloadPassKitResult[];
extern const char kUmaDownloadBundledPassKitResult[];

// Enum for the Download.IOSDownloadPassKitResult UMA histogram to report the
// results of the PassKit download.
// Note: This enum is used to back the DownloadPassKitResult UMA enum, and
// should be treated as append-only.
// LINT.IfChange
enum class DownloadPassKitResult {
  kSuccessful = 0,
  // PassKit download failed for a reason other than wrong MIME type or 401/403
  // HTTP response.
  kOtherFailure = 1,
  // PassKit download failed due to either a 401 or 403 HTTP response.
  kUnauthorizedFailure = 2,
  // PassKit download did not download the correct MIME type. This can happen
  // when web server redirects to login page instead of returning PassKit data.
  kWrongMimeTypeFailure = 3,
  // There was a failure when parsing pass kit data.
  kParsingFailure = 4,
  // There was a failure when parsing some pass kit data, but the parsing
  // succeeded on some passes.
  kPartialFailure = 5,
  kMaxValue = kPartialFailure,
};
// LINT.ThenChange(/tools/metrics/histograms/enums.xml)

// TabHelper which downloads pkpass file, constructs PKPass object and passes
// that PKPass to the delegate.
class PassKitTabHelper : public web::LazyWebStateUserData<PassKitTabHelper>,
                         public web::DownloadTaskObserver {
 public:
  PassKitTabHelper(const PassKitTabHelper&) = delete;
  PassKitTabHelper& operator=(const PassKitTabHelper&) = delete;

  ~PassKitTabHelper() override;

  // Asynchronously downloads pkpass file using the given `task`. Asks delegate
  // to present "Add pkpass" dialog when the download is complete.
  virtual void Download(std::unique_ptr<web::DownloadTask> task);

  // Set the web content handler, used to display the passkit UI.
  void SetWebContentsHandler(id<WebContentCommands> handler);

 protected:
  // Allow subclassing from PassKitTabHelper for testing purposes.
  explicit PassKitTabHelper(web::WebState* web_state);

 private:
  friend class web::LazyWebStateUserData<PassKitTabHelper>;

  // web::DownloadTaskObserver overrides:
  void OnDownloadUpdated(web::DownloadTask* task) override;

  // Called when the downloaded data of bundled passkits is available.
  void OnDownloadBundledPassesDataRead(DownloadPassKitResult uma_result,
                                       NSData* data);

  // Called when the downloaded data of a single passkit is available.
  void OnDownloadPassDataRead(DownloadPassKitResult uma_result, NSData* data);

  // Called when all the downloaded data are available.
  void OnDownloadDataAllRead(std::string uma_histogram,
                             DownloadPassKitResult uma_result,
                             NSArray<NSData*>* all_data);

  raw_ptr<web::WebState> web_state_;
  __weak id<WebContentCommands> handler_ = nil;
  // Set of unfinished download tasks.
  std::set<std::unique_ptr<web::DownloadTask>, base::UniquePtrComparator>
      tasks_;

  // Util used for unzipping through JavaScript.
  JSUnzipper* unzipper_;

  base::WeakPtrFactory<PassKitTabHelper> weak_factory_{this};

  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_MODEL_PASS_KIT_TAB_HELPER_H_
