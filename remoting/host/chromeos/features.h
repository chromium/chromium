// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CHROMEOS_FEATURES_H_
#define REMOTING_HOST_CHROMEOS_FEATURES_H_

#include "base/feature_list.h"

namespace remoting::features {

// Enable to allow CRD to stream other monitors than the primary display.
extern const base::Feature kEnableMultiMonitorsInCrd;
// Enable to allow CRD remote admin connections when the ChromeOS device is at
// the login screen.
extern const base::Feature kEnableCrdAdminRemoteAccess;
// Force all enterprise remote connections to be remote access connections.
// Only used for local testing until the DPanel UI supports sending remote
// access requests.
extern const base::Feature kForceCrdAdminRemoteAccess;

// Enable to allow FrameSinkDesktopCapturer to be used for CRD video streaming.
extern const base::Feature kEnableFrameSinkDesktopCapturerInCrd;

}  // namespace remoting::features

#endif  // REMOTING_HOST_CHROMEOS_FEATURES_H_
