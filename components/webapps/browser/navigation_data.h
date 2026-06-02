// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_NAVIGATION_DATA_H_
#define COMPONENTS_WEBAPPS_BROWSER_NAVIGATION_DATA_H_

#include <memory>
#include <optional>

#include "components/webapps/browser/launch_queue/launch_params.h"

namespace webapps {

// A bundle of webapps-related data that may optional be stored on
// NavigateParams.
class NavigationData {
 public:
  NavigationData();
  ~NavigationData();
  NavigationData(const NavigationData&) = delete;
  NavigationData& operator=(NavigationData&&) = delete;
  NavigationData(NavigationData&&);

  // This option forces PWA navigation capturing (which captures some
  // navigations into PWA windows or tabs) off. This is only recommended to be
  // used if the navigation MUST not be captured. See
  // https://bit.ly/pwa-navigation-capturing for a description about what PWA
  // navigation capturing does. Setting this field to `true` will disable all of
  // the behaviors listed in that document.
  bool navigation_capturing_force_off() const;
  void SetNavigationCapturingForceOff(bool force_off);

  // Optional launch parameters to be attached to the resulting navigation, once
  // the navigation commits.
  std::optional<LaunchParams> launch_params() const;
  void SetLaunchParams(LaunchParams launch_params);

 private:
  bool navigation_capturing_force_off_ = false;
  std::optional<LaunchParams> launch_params_ = std::nullopt;
};

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_NAVIGATION_DATA_H_
