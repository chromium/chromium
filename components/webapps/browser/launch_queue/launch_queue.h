// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_LAUNCH_QUEUE_LAUNCH_QUEUE_H_
#define COMPONENTS_WEBAPPS_BROWSER_LAUNCH_QUEUE_LAUNCH_QUEUE_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/webapps/browser/launch_queue/launch_params.h"
#include "components/webapps/common/web_app_id.h"

class GURL;

namespace content {
class WebContents;
}  // namespace content

namespace webapps {
class LaunchQueueDelegate;

// This handles passing LaunchParams through to its WebContents.
// There are three scenarios in which launch params are sent to a WebContents:
// - Navigating launches: The launch params are stored until the navigation
//   completes.
//
// - Non-navigating launches: The launch params are sent immediately to the
//   WebContents unless there was a navigating launch still pending, then it
//   gets added to the queue.
//
// - Page reloads: The last launch params sent to the page get resent.
//   Note that this is not in the spec and a bit magical. This is to cater for
//   the scenario where a user opens a web app via a file handler which provides
//   a file handle to the app. Without this reload mechanism the page would lose
//   access to the file handle if the user were to refresh the page.
class LaunchQueue {
 public:
  LaunchQueue(content::WebContents* web_contents,
              std::unique_ptr<LaunchQueueDelegate> delegate);

  LaunchQueue(const LaunchQueue&) = delete;
  LaunchQueue& operator=(const LaunchQueue&) = delete;

  ~LaunchQueue();

  void Enqueue(LaunchParams launch_params);

  bool IsInScope(const LaunchParams& launch_params, const GURL& url) const;

  void FlushForTesting() const;

 private:
  void SendLaunchParams(LaunchParams launch_params, const GURL& current_url);

  raw_ptr<content::WebContents> web_contents_;

  std::unique_ptr<LaunchQueueDelegate> delegate_;
};

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_LAUNCH_QUEUE_LAUNCH_QUEUE_H_
