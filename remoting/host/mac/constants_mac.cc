// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/mac/constants_mac.h"

#include "remoting/host/version.h"

namespace remoting {

#define SERVICE_NAME "org.chromium.chromoting"
#define BROKER_NAME SERVICE_NAME ".broker"

#define APPLICATIONS_DIR "/Applications/"
#define HELPER_TOOLS_DIR "/Library/PrivilegedHelperTools/"
#define LAUNCH_AGENTS_DIR "/Library/LaunchAgents/"
#define LAUNCH_DAEMONS_DIR "/Library/LaunchDaemons/"
#define LOG_DIR "/var/log/"
#define LOG_CONFIG_DIR "/etc/newsyslog.d/"

const char kBundleId[] = HOST_BUNDLE_ID;
const char kServiceName[] = SERVICE_NAME;
const char kBrokerName[] = BROKER_NAME;

const char kHostConfigFileName[] = SERVICE_NAME ".json";
const char kHostConfigFilePath[] = HELPER_TOOLS_DIR SERVICE_NAME ".json";

// Note: If this path is changed, also update the value set in:
// //remoting/base/file_host_settings_mac.cc
const char kHostSettingsFilePath[] =
    HELPER_TOOLS_DIR SERVICE_NAME ".settings.json";

const char kHostServiceBinaryPath[] = HELPER_TOOLS_DIR HOST_BUNDLE_NAME
    "/Contents/MacOS/remoting_me2me_host_service";
const char kOldHostHelperScriptPath[] =
    HELPER_TOOLS_DIR SERVICE_NAME ".me2me.sh";
const char kHostBinaryPath[] = HELPER_TOOLS_DIR HOST_BUNDLE_NAME;
const char kHostLegacyBinaryPath[] = HELPER_TOOLS_DIR HOST_LEGACY_BUNDLE_NAME;
const char kHostEnabledPath[] = HELPER_TOOLS_DIR SERVICE_NAME ".me2me_enabled";

const char kServicePlistPath[] = LAUNCH_AGENTS_DIR SERVICE_NAME ".plist";

const char kBrokerPlistPath[] = LAUNCH_DAEMONS_DIR BROKER_NAME ".plist";

const char kLogFilePath[] = LOG_DIR SERVICE_NAME ".log";
const char kLogFileConfigPath[] = LOG_CONFIG_DIR SERVICE_NAME ".conf";

const char* kNativeMessagingManifestPaths[4] = {
    "/Library/Google/Chrome/NativeMessagingHosts/"
    "com.google.chrome.remote_desktop.json",
    "/Library/Google/Chrome/NativeMessagingHosts/"
    "com.google.chrome.remote_assistance.json",
    "/Library/Application Support/Mozilla/NativeMessagingHosts/"
    "com.google.chrome.remote_desktop.json",
    "/Library/Application Support/Mozilla/NativeMessagingHosts/"
    "com.google.chrome.remote_assistance.json",
};

const char kBrandedUninstallerPath[] =
    APPLICATIONS_DIR "Chrome Remote Desktop Host Uninstaller.app";
const char kUnbrandedUninstallerPath[] =
    APPLICATIONS_DIR "Chromoting Host Uninstaller.app";

}  // namespace remoting
