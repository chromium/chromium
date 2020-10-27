// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_NETWORK_CONTENT_SECURITY_POLICY_PARSERS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_NETWORK_CONTENT_SECURITY_POLICY_PARSERS_H_

#include "third_party/blink/renderer/platform/crypto.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/text/unicode.h"

namespace blink {

typedef std::pair<unsigned, DigestValue> CSPHashValue;

enum ContentSecurityPolicyHashAlgorithm {
  kContentSecurityPolicyHashAlgorithmNone = 0,
  kContentSecurityPolicyHashAlgorithmSha256 = 1 << 2,
  kContentSecurityPolicyHashAlgorithmSha384 = 1 << 3,
  kContentSecurityPolicyHashAlgorithmSha512 = 1 << 4
};

PLATFORM_EXPORT bool IsCSPDirectiveNameCharacter(UChar);
PLATFORM_EXPORT bool IsCSPDirectiveValueCharacter(UChar);
PLATFORM_EXPORT bool IsNonceCharacter(UChar);
PLATFORM_EXPORT bool IsSourceCharacter(UChar);
PLATFORM_EXPORT bool IsPathComponentCharacter(UChar);
PLATFORM_EXPORT bool IsHostCharacter(UChar);
PLATFORM_EXPORT bool IsSchemeContinuationCharacter(UChar);
PLATFORM_EXPORT bool IsNotASCIISpace(UChar);
PLATFORM_EXPORT bool IsNotColonOrSlash(UChar);
PLATFORM_EXPORT bool IsMediaTypeCharacter(UChar);

// Only checks for general Base64 encoded chars, not '=' chars since '=' is
// positional and may only appear at the end of a Base64 encoded string.
PLATFORM_EXPORT bool IsBase64EncodedCharacter(UChar);

}  // namespace blink

#endif
