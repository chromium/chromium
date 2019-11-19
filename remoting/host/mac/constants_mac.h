// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CONSTANTS_MAC_H_
#define REMOTING_HOST_CONSTANTS_MAC_H_

namespace remoting {

// The name of the Remoting Host service that is registered with launchd.
extern const char kServiceName[];

// Use a single configuration file, instead of separate "auth" and "host" files.
// This is because the SetConfigAndStart() API only provides a single
// dictionary, and splitting this into two dictionaries would require
// knowledge of which keys belong in which files.
extern const char kHostConfigFileName[];
extern const char kHostConfigFilePath[];

// This helper script is executed as root to enable/disable/configure the host
// service.
// It is also used (as non-root) to provide version information for the
// installed host components.
extern const char kHostServiceBinaryPath[];

// Path to the old host helper script, which is still used after user updates
// their host on macOS 10.14.*.
extern const char kOldHostHelperScriptPath[];

// Path to the service binary (.app).
extern const char kHostBinaryPath[];

// Path to the legacy service binary (.bundle).
extern const char kHostLegacyBinaryPath[];

// If this file exists, it means that the host is enabled for sharing.
extern const char kHostEnabledPath[];

// The .plist file for the Chromoting service.
extern const char kServicePlistPath[];

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

#endif  // REMOTING_HOST_CONSTANTS_MAC_H_
