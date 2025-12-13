// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

enum DisplayCecPowerState {
  // There was an error querying the power state of the display.
  "error",

  // The kernel adapter for the CEC endpoint isn't configured (no EDID set).
  "adapterNotConfigured",

  // No device ACKed the request on the CEC bus.
  "noDevice",

  // The display is a powered on state.
  "on",

  // The display is in standby mode.
  "standby",

  // The display is currently transitioning to an awake state. It can't be
  // relied on to show any output yet.
  "transitioningToOn",

  // The display is currently transitioning to standby.
  "transitioningToStandby",

  // Found a CEC endpoint but unable to determine the power state.
  "unknown"
};

// Private API for HDMI CEC functionality.
[platforms=("chromeos")]
interface CecPrivate {
  // Attempt to put all HDMI CEC compatible devices in standby.
  //
  // This is not guaranteed to have any effect on the connected displays.
  // Displays that do not support HDMI CEC will not be affected.
  //
  // |Returns|: will be run as soon as all displays have been requested to
  // change their power state.
  static Promise<undefined> sendStandBy();

  // Attempt to announce this device as the active input source towards all
  // HDMI CEC enabled displays connected, waking them from standby if
  // necessary.
  //
  // |Returns|: will be run as soon as all displays have been requested to
  // change their power state.
  static Promise<undefined> sendWakeUp();

  // Queries all HDMI CEC capable displays for their current power state.
  // |PromiseValue|: powerStates
  [requiredCallback] static Promise<sequence<DisplayCecPowerState>>
      queryDisplayCecPowerState();
};

partial interface Browser {
  static attribute CecPrivate cecPrivate;
};
