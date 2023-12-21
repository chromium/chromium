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

// Enable the V2 feature development related to launching CRD remote admin
// to GA.
BASE_DECLARE_FEATURE(kEnableCrdAdminRemoteAccessV2);

// Enable to allow file transfer in CRD video streaming to Kiosk devices.
BASE_DECLARE_FEATURE(kEnableCrdFileTransferForKiosk);

}  // namespace remoting::features

#endif  // REMOTING_HOST_CHROMEOS_FEATURES_H_
