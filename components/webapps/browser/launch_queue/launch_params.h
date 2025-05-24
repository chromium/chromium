// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_LAUNCH_QUEUE_LAUNCH_PARAMS_H_
#define COMPONENTS_WEBAPPS_BROWSER_LAUNCH_QUEUE_LAUNCH_PARAMS_H_

#include <vector>

#include "base/files/file_path.h"
#include "base/time/time.h"
#include "components/webapps/common/web_app_id.h"
#include "url/gurl.h"

namespace webapps {

// Represents the LaunchParams values to be sent to a web app on app launch and
// whether the launch triggered a navigation or not.
struct LaunchParams {
  LaunchParams();
  LaunchParams(const LaunchParams&);
  LaunchParams(LaunchParams&&) noexcept;

  ~LaunchParams();

  LaunchParams& operator=(const LaunchParams&);
  LaunchParams& operator=(LaunchParams&&) noexcept;

  // Whether this launch triggered a navigation that needs to be awaited before
  // sending the launch params to the document.
  bool started_new_navigation = true;

  // The app being launched, used for scope validation.
  webapps::AppId app_id;

  // The URL the web app was launched with. Note that redirects may cause us to
  // enqueue in a different URL, we still report the original launch target URL
  // in the launch params.
  GURL target_url;

  // The directory to launch with (may be empty).
  base::FilePath dir;

  // The files to launch with (may be empty).
  std::vector<base::FilePath> paths;

  // Stores the time when the browser process receives the navigation that
  // causes the `LaunchParams` to be created.
  base::TimeTicks time_navigation_started_for_enqueue;
};

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_LAUNCH_QUEUE_LAUNCH_PARAMS_H_
