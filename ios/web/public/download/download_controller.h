// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_DOWNLOAD_DOWNLOAD_CONTROLLER_H_
#define IOS_WEB_PUBLIC_DOWNLOAD_DOWNLOAD_CONTROLLER_H_

#include <Foundation/Foundation.h>

#include <string>

#include "ui/base/page_transition_types.h"

@class DownloadNativeTaskBridge;
class GURL;

namespace web {

class BrowserState;
class DownloadControllerDelegate;
class DownloadTask;
class WebState;

// Provides API for browser downloads. Must be used on the UI thread. Allows
// handling renderer-initiated download tasks and resuming unfinished downloads
// after the application relaunch.
//
// Handling renderer-initiated downloads example:
// class MyDownloadManager : public DownloadTaskObserver,
//                           public DownloadControllerDelegate {
//  public:
//   MyDownloadManager(BrowserState* state) : state_(state) {
//     DownloadController::FromBrowserState(state)->SetDelegate(this);
//   }
//   void OnDownloadCreated(DownloadController*,
//                          WebState*,
//                          std::unique_ptr<DownloadTask> task) override {
//     task->AddObserver(this);
//     task->Start(GetURLFetcherFileWriter());
//     _task = std::move(task);
//   }
//   void OnDownloadUpdated(DownloadTask* task) override {
//     if (task->IsDone()) {
//       _task->RemoveObserver(this);
//       _task = nullptr;
//     } else {
//       ShowProgress(task->GetPercentComplete());
//     }
//   }
//   void OnDownloadControllerDestroyed(DownloadController*) override {
//     DownloadController::FromBrowserState(state_)->SetDelegate(nullptr);
//   }
//  private:
//    BrowserState* state_;
//    unique_ptr<DownloadTask> task_;
// };
//
// Resuming unfinished downloads after application relaunch example:
// @interface MyAppDelegate : UIResponder <UIApplicationDelegate>
// @end
// @implementation MyAppDelegate
// - (void)application:(UIApplication *)application
//     handleEventsForBackgroundURLSession:(NSString *)identifier
//                       completionHandler:(void (^)())completionHandler {
//   MyDownloadInfo* info = [self storedDownloadInfoForIdentifier:identifier];
//   DownloadController::FromBrowserState(self.state)->CreateDownloadTask(
//       [self webStateAtIndex:info.webStateIndex],
//       identifier,
//       info.originalURL,
//       info.originalHTTPMethod,
//       info.contentDisposition,
//       info.totalBytes,
//       info.MIMEType);
//   );
// }
// - (void)applicationWillTerminate:(UIApplication *)application {
//   for (DownloadTask* task : self.downloadTasks) {
//     [MyDownloadInfo storeDownloadInfoForTask:task];
//   }
// }
// @end
//
class DownloadController {
 public:
  // Returns DownloadController for the given `browser_state`. `browser_state`
  // must not be null.
  static DownloadController* FromBrowserState(BrowserState* browser_state);

  // Creates a new download task. Clients may call this method to resume the
  // download after the application relaunch or start a new download. Clients
  // must not call this method to initiate a renderer-initiated download (those
  // downloads are created automatically).
  // In order to resume the download after the application relaunch clients have
  // to pass `identifier` obtained from
  // application:handleEventsForBackgroundURLSession:completionHandler:
  // UIApplicationDelegate callback. The rest of arguments should be taken
  // from DownloadTask, which was suspended when the application has been
  // terminated. In order to resume the download, clients must persist all
  // DownloadTask data for each unfinished download on disk.
  virtual void CreateDownloadTask(WebState* web_state,
                                  NSString* identifier,
                                  const GURL& original_url,
                                  NSString* http_method,
                                  const std::string& content_disposition,
                                  int64_t total_bytes,
                                  const std::string& mime_type) = 0;

  // Creates a new native download task. This method uses `download` which
  // is used to perform downloads using WKDownload instead of NSURLSession
  // and will be supported from iOS 15+.
  virtual void CreateNativeDownloadTask(WebState* web_state,
                                        NSString* identifier,
                                        const GURL& original_url,
                                        NSString* http_method,
                                        const std::string& content_disposition,
                                        int64_t total_bytes,
                                        const std::string& mime_type,
                                        DownloadNativeTaskBridge* download) = 0;

  // Sets DownloadControllerDelegate. Clients must set the delegate to null in
  // DownloadControllerDelegate::OnDownloadControllerDestroyed().
  virtual void SetDelegate(DownloadControllerDelegate* delegate) = 0;

  // Returns DownloadControllerDelegate.
  virtual DownloadControllerDelegate* GetDelegate() const = 0;

  DownloadController() = default;

  DownloadController(const DownloadController&) = delete;
  DownloadController& operator=(const DownloadController&) = delete;

  virtual ~DownloadController() = default;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_DOWNLOAD_DOWNLOAD_CONTROLLER_H_
