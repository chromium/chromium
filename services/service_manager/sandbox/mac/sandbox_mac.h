// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICE_MANAGER_SANDBOX_MAC_SANDBOX_MAC_H_
#define SERVICE_MANAGER_SANDBOX_MAC_SANDBOX_MAC_H_

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "services/service_manager/sandbox/export.h"
#include "services/service_manager/sandbox/sandbox_type.h"

namespace base {
class FilePath;
}

namespace service_manager {

class SERVICE_MANAGER_SANDBOX_EXPORT SandboxMac {
 public:
  // Warm up System APIs that empirically need to be accessed before the
  // sandbox is turned on. |sandbox_type| is the type of sandbox to warm up.
  // Valid |sandbox_type| values are defined by the enum SandboxType, or can be
  // defined by the embedder via
  // ContentClient::GetSandboxProfileForProcessType().
  static void Warmup(SandboxType sandbox_type);

  // Turns on the OS X sandbox for this process.
  // |sandbox_type| - type of Sandbox to use. See SandboxWarmup() for legal
  // values.
  //
  // Returns true on success, false if an error occurred enabling the sandbox.
  static bool Enable(SandboxType sandbox_type);

  // Convert provided path into a "canonical" path matching what the Sandbox
  // expects i.e. one without symlinks.
  // This path is not necessarily unique e.g. in the face of hardlinks.
  static base::FilePath GetCanonicalPath(const base::FilePath& path);

  // Returns the sandbox profile string for a given sandbox type.
  // It CHECKs that the sandbox profile is a valid type, so it always returns a
  // valid result, or crashes.
  static std::string GetSandboxProfile(SandboxType sandbox_type);

  static const char* kSandboxBrowserPID;
  static const char* kSandboxBundlePath;
  static const char* kSandboxChromeBundleId;
  static const char* kSandboxComponentPath;
  static const char* kSandboxDisableDenialLogging;
  static const char* kSandboxEnableLogging;
  static const char* kSandboxHomedirAsLiteral;
  static const char* kSandboxLoggingPathAsLiteral;
  static const char* kSandboxOSVersion;

  // TODO(kerrnel): this is only for the legacy sandbox.
  static const char* kSandboxElCapOrLater;
  static const char* kSandboxMacOS1013;
  static const char* kSandboxFieldTrialSeverName;

  static const char* kSandboxBundleVersionPath;

 private:
  FRIEND_TEST_ALL_PREFIXES(MacDirAccessSandboxTest, StringEscape);
  FRIEND_TEST_ALL_PREFIXES(MacDirAccessSandboxTest, RegexEscape);
  FRIEND_TEST_ALL_PREFIXES(MacDirAccessSandboxTest, SandboxAccess);

  DISALLOW_IMPLICIT_CONSTRUCTORS(SandboxMac);
};

}  // namespace service_manager

#endif  // SERVICE_MANAGER_SANDBOX_MAC_SANDBOX_MAC_H_
