// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_MAC_CONSTANTS_MAC_H_
#define REMOTING_HOST_MAC_CONSTANTS_MAC_H_

namespace remoting {

// The bundle ID for the Remoting Host.
extern const char kBundleId[];

// The name of the Remoting Host service that is registered with launchd.
extern const char kServiceName[];

// The name of the Remoting Host broker that is registered with launchd.
extern const char kBrokerName[];

// Use a single configuration file, instead of separate "auth" and "host" files.
// This is because the SetConfigAndStart() API only provides a single
// dictionary, and splitting this into two dictionaries would require
// knowledge of which keys belong in which files.
extern const char kHostConfigFileName[];
extern const char kHostConfigFilePath[];

// File that stores lightweight host settings as key-value pairs. See
// HostSettings for more info.
extern const char kHostSettingsFilePath[];

// This helper script is executed as root to enable/disable/configure the host
// service.
// It is also used (as non-root) to provide version information for the
// installed host components.
extern const char kHostServiceBinaryPath[];

// Path to the old host helper script, which is still used after user updates
// their host on macOS 10.14.*. TODO(crbug.com/40275162): Remove.
extern const char kOldHostHelperScriptPath[];

// Path to the service binary (.app).
extern const char kHostBinaryPath[];

// Path to the legacy service binary (.bundle).
extern const char kHostLegacyBinaryPath[];

// If this file exists, it means that the host is enabled for sharing.
extern const char kHostEnabledPath[];

// The .plist file for the Chromoting service.
extern const char kServicePlistPath[];

// The .plist file for the Chromoting agent broker.
extern const char kBrokerPlistPath[];

// Path to the host log file
extern const char kLogFilePath[];

// Path to the log config file
extern const char kLogFileConfigPath[];

// Paths to the native messaging host manifests
extern const char* kNativeMessagingManifestPaths[4];

// The branded and unbranded names for the uninstaller.
// This is the only file that changes names based on branding. We define both
// because we want local dev builds to be able to clean up both files.
extern const char kBrandedUninstallerPath[];
extern const char kUnbrandedUninstallerPath[];

}  // namespace remoting

#endif  // REMOTING_HOST_MAC_CONSTANTS_MAC_H_
