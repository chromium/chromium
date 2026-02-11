// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WEBORIGIN_SANDBOXED_OPAQUE_SECURITY_ORIGIN_CREATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WEBORIGIN_SANDBOXED_OPAQUE_SECURITY_ORIGIN_CREATOR_H_

#include "base/memory/scoped_refptr.h"
#include "base/types/pass_key.h"
#include "base/unguessable_token.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {

class DocumentLoader;
class SecurityOrigin;

// SandboxedOpaqueSecurityOriginCreator creates opaque SecurityOrigin instances
// with specified nonces for sandboxed frames. This class should NOT be used
// for any other origin creation purposes.
class PLATFORM_EXPORT SandboxedOpaqueSecurityOriginCreator {
 public:
  // Creates an opaque SecurityOrigin for sandboxed frames with the given nonce
  // and origin. Only callable from DocumentLoader.
  static scoped_refptr<SecurityOrigin> CreateOriginForSandboxedFrame(
      base::PassKey<DocumentLoader>,
      const base::UnguessableToken& nonce,
      const SecurityOrigin* origin);

  // Disable instantiation
  SandboxedOpaqueSecurityOriginCreator() = delete;
  ~SandboxedOpaqueSecurityOriginCreator() = delete;
  SandboxedOpaqueSecurityOriginCreator(
      const SandboxedOpaqueSecurityOriginCreator&) = delete;
  SandboxedOpaqueSecurityOriginCreator& operator=(
      const SandboxedOpaqueSecurityOriginCreator&) = delete;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WEBORIGIN_SANDBOXED_OPAQUE_SECURITY_ORIGIN_CREATOR_H_
