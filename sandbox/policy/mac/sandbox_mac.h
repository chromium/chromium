// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_POLICY_MAC_SANDBOX_MAC_H_
#define SANDBOX_POLICY_MAC_SANDBOX_MAC_H_

#include <string>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "sandbox/policy/export.h"
#include "sandbox/policy/sandbox_type.h"

namespace base {
class FilePath;
}

namespace sandbox {
namespace policy {

class SANDBOX_POLICY_EXPORT SandboxMac {
 public:
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
  static const char* kSandboxSodaComponentPath;
  static const char* kSandboxSodaLanguagePackPath;
  static const char* kSandboxComponentPath;
  static const char* kSandboxDisableDenialLogging;
  static const char* kSandboxEnableLogging;
  static const char* kSandboxHomedirAsLiteral;
  static const char* kSandboxLoggingPathAsLiteral;
  static const char* kSandboxOSVersion;

  static const char* kSandboxBundleVersionPath;
  static const char* kSandboxDisableMetalShaderCache;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(SandboxMac);
};

}  // namespace policy
}  // namespace sandbox

#endif  // SANDBOX_POLICY_MAC_SANDBOX_MAC_H_
