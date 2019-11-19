// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/switches.h"

#include "build/build_config.h"

namespace remoting {

const char kElevateSwitchName[] = "elevate";
const char kHelpSwitchName[] = "help";
const char kProcessTypeSwitchName[] = "type";
const char kQuestionSwitchName[] = "?";
const char kVersionSwitchName[] = "version";

const char kProcessTypeController[] = "controller";
const char kProcessTypeDaemon[] = "daemon";
const char kProcessTypeDesktop[] = "desktop";
const char kProcessTypeHost[] = "host";
const char kProcessTypeRdpDesktopSession[] = "rdp_desktop_session";
const char kProcessTypeEvaluateCapability[] = "evaluate_capability";
const char kProcessTypeFileChooser[] = "file_chooser";
#if defined(OS_LINUX)
const char kProcessTypeXSessionChooser[] = "xsession_chooser";
#endif  // defined(OS_LINUX)

const char kEvaluateCapabilitySwitchName[] = "evaluate-type";

#if defined(OS_WIN)
const char kEvaluateD3D[] = "d3d-support";
const char kEvaluate3dDisplayMode[] = "3d-display-mode";
#endif

const char kParentWindowSwitchName[] = "parent-window";

const char kInputSwitchName[] = "input";
const char kOutputSwitchName[] = "output";

const char kMojoPipeToken[] = "mojo-pipe-token";

const char kUpgradeTokenSwitchName[] = "upgrade-token";

#if defined(OS_MACOSX)
const char kCheckPermissionSwitchName[] = "check-permission";
const char kCheckAccessibilityPermissionSwitchName[] =
    "check-accessibility-permission";
const char kCheckScreenRecordingPermissionSwitchName[] =
    "check-screen-recording-permission";
#endif  // defined OS_MACOSX

}  // namespace remoting
