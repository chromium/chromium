// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/mojo_caller_security_checker.h"

#include <array>
#include <memory>

#include "base/containers/fixed_flat_set.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/process/process_handle.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "build/build_config.h"
#include "components/named_mojo_ipc_server/connection_info.h"
#include "remoting/host/base/process_util.h"

#if BUILDFLAG(IS_MAC)
#include <Security/Security.h>

#include "base/apple/osstatus_logging.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/mac/code_signature.h"
#include "base/strings/stringprintf.h"
#include "remoting/host/mac/constants_mac.h"
#include "remoting/host/version.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "remoting/host/win/trust_util.h"
#endif

namespace remoting {
namespace {

#if BUILDFLAG(IS_LINUX)
constexpr auto kAllowedCallerProgramNames =
    base::MakeFixedFlatSet<base::FilePath::StringPieceType>({
        "remote-open-url",
        "remote-webauthn",
    });
#elif BUILDFLAG(IS_WIN)
constexpr auto kAllowedCallerProgramNames =
    base::MakeFixedFlatSet<base::FilePath::StringPieceType>({
        L"remote_open_url.exe",
        L"remote_webauthn.exe",
        L"remote_security_key.exe",
    });
#endif

}  // namespace

bool IsTrustedMojoEndpoint(
    const named_mojo_ipc_server::ConnectionInfo& caller) {
#if BUILDFLAG(IS_MAC)

#if defined(OFFICIAL_BUILD)
  std::string requirement_string = base::StringPrintf(
      // Certificate was issued by Apple
      "anchor apple generic and "
      // It's Google's certificate
      "certificate leaf[subject.OU] = \"%s\" and "
      // See:
      // https://developer.apple.com/documentation/technotes/tn3127-inside-code-signing-requirements#Xcode-designated-requirement-for-Developer-ID-code
      "certificate 1[field.1.2.840.113635.100.6.2.6] and "
      "certificate leaf[field.1.2.840.113635.100.6.1.13] and "
      // For Chrome Remote Desktop
      "identifier \"%s\"",
      MAC_TEAM_ID, kBundleId);
  base::apple::ScopedCFTypeRef<SecRequirementRef> requirement;
  OSStatus status = SecRequirementCreateWithString(
      base::SysUTF8ToCFStringRef(requirement_string).get(), kSecCSDefaultFlags,
      requirement.InitializeInto());
  if (status != errSecSuccess) {
    OSSTATUS_LOG(ERROR, status)
        << "Failed to create security requirement for string: "
        << requirement_string;
    return false;
  }
  status = base::mac::ProcessIsSignedAndFulfillsRequirement(caller.audit_token,
                                                            requirement.get());
  if (status == errSecSuccess) {
    return true;
  }
  if (status == errSecCSReqFailed) {
    OSSTATUS_LOG(ERROR, status) << "Security requirement unsatisfied";
  } else {
    OSSTATUS_LOG(ERROR, status)
        << "Unknown error occurred when verifying security requirements";
  }
  return false;
#else
  // Skip codesign verification to allow for local development.
  return true;
#endif

#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)

  // TODO: yuweih - see if it's possible to move away from PID-based security
  // checks, which might be susceptible of PID reuse attacks.
  static base::NoDestructor<base::FilePath> current_process_image_path(
      GetProcessImagePath(base::GetCurrentProcId()));
  base::FilePath caller_process_image_path = GetProcessImagePath(caller.pid);
  if (caller_process_image_path.empty()) {
    LOG(ERROR) << "Cannot resolve process image path for PID " << caller.pid;
    return false;
  }
  if (caller_process_image_path == *current_process_image_path) {
    // IPCs initiated from the same binary should be allowed.
    return true;
  }
  if (caller_process_image_path.DirName() !=
      current_process_image_path->DirName()) {
    LOG(ERROR) << "Process image " << caller_process_image_path
               << " is not under " << current_process_image_path->DirName();
    return false;
  }
  base::FilePath::StringType program_name =
      caller_process_image_path.BaseName().value();
  if (!kAllowedCallerProgramNames.contains(program_name)) {
#if BUILDFLAG(IS_LINUX) && !defined(NDEBUG)
    // Linux binaries generated in out/Debug are underscore-separated. To make
    // debugging easier, we just check the name again with underscores replaced
    // with hyphens.
    std::string program_name_hyphenated;
    base::ReplaceChars(program_name, "_", "-", &program_name_hyphenated);
    if (kAllowedCallerProgramNames.contains(program_name_hyphenated)) {
      return true;
    }
#endif  // BUILDFLAG(IS_LINUX) && !defined(NDEBUG)
    LOG(ERROR) << caller_process_image_path.BaseName()
               << " is not in the list of allowed caller programs.";
    return false;
  }
#if BUILDFLAG(IS_WIN)
  return IsBinaryTrusted(caller_process_image_path);
#else
  // Linux binaries are not code-signed, so we just return true.
  return true;
#endif

#else  // Unsupported platform
  NOTREACHED();
#endif
}

}  // namespace remoting
