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
class LaunchParams {
 public:
  LaunchParams();
  LaunchParams(const LaunchParams&);
  LaunchParams(LaunchParams&&) noexcept;

  ~LaunchParams();

  LaunchParams& operator=(const LaunchParams&);
  LaunchParams& operator=(LaunchParams&&) noexcept;

  // Getters
  bool started_new_navigation() const { return started_new_navigation_; }
  const webapps::AppId& app_id() const { return app_id_; }
  const GURL& target_url() const { return target_url_; }
  const base::FilePath& dir() const { return dir_; }
  const std::vector<base::FilePath>& paths() const { return paths_; }
  base::TimeTicks time_navigation_started_for_enqueue() const {
    return time_navigation_started_for_enqueue_;
  }

  // Setters
  void set_started_new_navigation(bool started_new_navigation) {
    started_new_navigation_ = started_new_navigation;
  }
  void set_app_id(webapps::AppId app_id) { app_id_ = std::move(app_id); }
  void set_target_url(GURL target_url) { target_url_ = std::move(target_url); }
  void set_dir(base::FilePath dir) { dir_ = std::move(dir); }
  void set_paths(std::vector<base::FilePath> paths) {
    paths_ = std::move(paths);
  }
  void set_time_navigation_started_for_enqueue(
      base::TimeTicks time_navigation_started_for_enqueue) {
    time_navigation_started_for_enqueue_ = time_navigation_started_for_enqueue;
  }

  // Mutation Helpers
  void clear_paths() { paths_.clear(); }
  void add_path(base::FilePath path) { paths_.push_back(std::move(path)); }
  void clear_dir() { dir_.clear(); }

 private:
  // Whether this launch triggered a navigation that needs to be awaited before
  // sending the launch params to the document.
  bool started_new_navigation_ = true;

  // The app being launched, used for scope validation.
  webapps::AppId app_id_;

  // The URL the web app was launched with. Note that redirects may cause us to
  // enqueue in a different URL, we still report the original launch target URL
  // in the launch params.
  GURL target_url_;

  // The directory to launch with (may be empty).
  base::FilePath dir_;

  // The files to launch with (may be empty).
  std::vector<base::FilePath> paths_;

  // Stores the time when the browser process receives the navigation that
  // causes the `LaunchParams` to be created.
  base::TimeTicks time_navigation_started_for_enqueue_;
};

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_LAUNCH_QUEUE_LAUNCH_PARAMS_H_
