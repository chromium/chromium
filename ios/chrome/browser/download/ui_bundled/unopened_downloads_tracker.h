// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <set>
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"
#import "ios/web/public/download/download_task_observer.h"

struct WebStateListStatus;
class WebStateListChangeDetach;

namespace web {
class DownloadTask;
}  // namespace web

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_UI_BUNDLED_UNOPENED_DOWNLOADS_TRACKER_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_UI_BUNDLED_UNOPENED_DOWNLOADS_TRACKER_H_

// Tracks download tasks which were not opened by the user yet. Reports various
// metrics in DownloadTaskObserver callbacks.
class UnopenedDownloadsTracker : public web::DownloadTaskObserver,
                                 public WebStateListObserver {
 public:
  UnopenedDownloadsTracker();
  UnopenedDownloadsTracker(const UnopenedDownloadsTracker&) = delete;
  UnopenedDownloadsTracker& operator=(const UnopenedDownloadsTracker&) = delete;
  ~UnopenedDownloadsTracker() override;

  // Starts tracking this download task.
  void Add(web::DownloadTask* task);
  // Stops tracking this download task.
  void Remove(web::DownloadTask* task);
  // DownloadTaskObserver overrides:
  void OnDownloadUpdated(web::DownloadTask* task) override;
  void OnDownloadDestroyed(web::DownloadTask* task) override;

  // Logs histograms. Called when DownloadTask or this object was destroyed.
  void DownloadAborted(web::DownloadTask* task);

  // WebStateListObserver overrides:
  void WebStateListWillChange(WebStateList* web_state_list,
                              const WebStateListChangeDetach& detach_change,
                              const WebStateListStatus& status) override;

 private:
  // True if a web state was closed without user action.
  bool did_close_web_state_without_user_action = false;
  // Keeps track of observed tasks to remove observer when
  // UnopenedDownloadsTracker is destructed.
  std::set<web::DownloadTask*> observed_tasks_;
};

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_UI_BUNDLED_UNOPENED_DOWNLOADS_TRACKER_H_
