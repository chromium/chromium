// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/base/switches.h"

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
const char kProcessTypeUrlForwarderConfigurator[] =
    "url_forwarder_configurator";
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
const char kProcessTypeXSessionChooser[] = "xsession_chooser";
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

const char kEvaluateCapabilitySwitchName[] = "evaluate-type";

#if BUILDFLAG(IS_LINUX)
const char kEnableUtempter[] = "enable-utempter";
#endif

#if BUILDFLAG(IS_WIN)
const char kEvaluateD3D[] = "d3d-support";
const char kEvaluate3dDisplayMode[] = "3d-display-mode";
const char kSetUpUrlForwarderSwitchName[] = "setup";
#endif

const char kParentWindowSwitchName[] = "parent-window";

const char kInputSwitchName[] = "input";
const char kOutputSwitchName[] = "output";

const char kMojoPipeToken[] = "mojo-pipe-token";
const char kCrashServerPipeHandle[] = "crash-server-pipe-handle";

#if BUILDFLAG(IS_APPLE)
const char kCheckPermissionSwitchName[] = "check-permission";
const char kCheckAccessibilityPermissionSwitchName[] =
    "check-accessibility-permission";
const char kCheckScreenRecordingPermissionSwitchName[] =
    "check-screen-recording-permission";
const char kListAudioDevicesSwitchName[] = "list-audio-devices";
#endif  // BUILDFLAG(IS_APPLE)

}  // namespace remoting
