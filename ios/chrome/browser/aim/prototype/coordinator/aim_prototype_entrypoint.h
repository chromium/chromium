// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AIM_PROTOTYPE_COORDINATOR_AIM_PROTOTYPE_ENTRYPOINT_H_
#define IOS_CHROME_BROWSER_AIM_PROTOTYPE_COORDINATOR_AIM_PROTOTYPE_ENTRYPOINT_H_

/// Enum defining invocation points for the AIM prototype.
enum class AIMPrototypeEntrypoint {
  /// The AIM button on NTP.
  kNTPAIMButton,
  /// The fakebox on NTP.
  kNTPFakebox,
  /// Other, commands from OmniboxCommand.
  kOther,
};

#endif  // IOS_CHROME_BROWSER_AIM_PROTOTYPE_COORDINATOR_AIM_PROTOTYPE_ENTRYPOINT_H_
