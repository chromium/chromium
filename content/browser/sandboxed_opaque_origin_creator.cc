// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/sandboxed_opaque_origin_creator.h"

#include "base/types/pass_key.h"
#include "url/origin.h"

namespace content {

// static
url::Origin SandboxedOpaqueOriginCreator::CreateOriginForSandboxedFrame(
    base::PassKey<RenderFrameHostImpl>,
    const base::UnguessableToken& nonce,
    const url::SchemeHostPort& tuple) {
  return url::Origin::CreateWithNonce(
      base::PassKey<SandboxedOpaqueOriginCreator>(), nonce, tuple);
}

// static
url::Origin
SandboxedOpaqueOriginCreator::CreateOriginForSandboxedFrameForTesting(
    base::PassKey<SandboxedOpaqueOriginCreatorTest>,
    const base::UnguessableToken& nonce,
    const url::SchemeHostPort& tuple) {
  return url::Origin::CreateWithNonce(
      base::PassKey<SandboxedOpaqueOriginCreator>(), nonce, tuple);  // IN-TEST
}

}  // namespace content
