// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/mac/trust_util.h"

#include <string>

#include "base/apple/osstatus_logging.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/mac/code_signature.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "remoting/host/version.h"

namespace remoting {

bool IsProcessTrusted(audit_token_t audit_token,
                      base::span<const std::string_view> identifiers) {
#if defined(OFFICIAL_BUILD)
  // See:
  // https://developer.apple.com/library/archive/documentation/Security/Conceptual/CodeSigningGuide/RequirementLang/RequirementLang.html
  DCHECK_GT(identifiers.size(), 0u);
  std::string identifier_requirements =
      "identifier \"" + base::JoinString(identifiers, "\" or identifier \"") +
      '"';
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
      "(%s)",
      MAC_TEAM_ID, identifier_requirements.data());
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
  status = base::mac::ProcessIsSignedAndFulfillsRequirement(audit_token,
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
}

}  // namespace remoting
