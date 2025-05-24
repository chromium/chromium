// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_LAUNCH_QUEUE_LAUNCH_QUEUE_DELEGATE_H_
#define COMPONENTS_WEBAPPS_BROWSER_LAUNCH_QUEUE_LAUNCH_QUEUE_DELEGATE_H_

class GURL;

namespace base {

class FilePath;

}  // namespace base

namespace content {

struct PathInfo;

}  // namespace content

namespace webapps {

struct LaunchParams;

class LaunchQueueDelegate {
 public:
  virtual ~LaunchQueueDelegate() = default;

  virtual bool IsInScope(const LaunchParams& launch_params,
                         const GURL& current_url) const = 0;

  virtual content::PathInfo GetPathInfo(
      const base::FilePath& entry_path) const = 0;

  virtual bool IsValidLaunchParams(const LaunchParams& params) const = 0;
};

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_LAUNCH_QUEUE_LAUNCH_QUEUE_DELEGATE_H_
