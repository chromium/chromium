// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/weborigin/sandboxed_opaque_security_origin_creator.h"

#include "base/types/pass_key.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

// static
scoped_refptr<SecurityOrigin>
SandboxedOpaqueSecurityOriginCreator::CreateOriginForSandboxedFrame(
    base::PassKey<DocumentLoader>,
    const base::UnguessableToken& nonce,
    const SecurityOrigin* origin) {
  return SecurityOrigin::CreateWithNonce(
      base::PassKey<SandboxedOpaqueSecurityOriginCreator>(), nonce, origin);
}

}  // namespace blink
