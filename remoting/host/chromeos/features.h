// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CHROMEOS_FEATURES_H_
#define REMOTING_HOST_CHROMEOS_FEATURES_H_

#include "base/feature_list.h"

namespace remoting::features {

// Enable to allow CRD remote admin connections when the ChromeOS device is at
// the login screen.
BASE_DECLARE_FEATURE(kEnableCrdAdminRemoteAccess);
// Force all enterprise remote connections to be remote access connections.
// Only used for local testing until the DPanel UI supports sending remote
// access requests.
BASE_DECLARE_FEATURE(kForceCrdAdminRemoteAccess);

// Enable to allow FrameSinkDesktopCapturer to be used for CRD video streaming.
BASE_DECLARE_FEATURE(kEnableFrameSinkDesktopCapturerInCrd);

}  // namespace remoting::features

#endif  // REMOTING_HOST_CHROMEOS_FEATURES_H_
