// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/chromeos/features.h"

#include "base/feature_list.h"

namespace remoting::features {

const base::Feature kEnableMultiMonitorsInCrd{"EnableMultiMonitorsInCrd",
                                              base::FEATURE_ENABLED_BY_DEFAULT};
const base::Feature kEnableCrdAdminRemoteAccess{
    "EnableCrdAdminRemoteAccess", base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kForceCrdAdminRemoteAccess{
    "ForceCrdAdminRemoteAccess", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kEnableFrameSinkDesktopCapturerInCrd{
    "EnableFrameSinkDesktopCapturerInCrd", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace remoting::features
