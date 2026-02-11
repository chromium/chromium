// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SANDBOXED_OPAQUE_ORIGIN_CREATOR_H_
#define CONTENT_BROWSER_SANDBOXED_OPAQUE_ORIGIN_CREATOR_H_

#include "base/types/pass_key.h"
#include "base/unguessable_token.h"
#include "content/common/content_export.h"
#include "url/scheme_host_port.h"

namespace url {
class Origin;
}

namespace content {

class RenderFrameHostImpl;
class SandboxedOpaqueOriginCreatorTest;

// SandboxedOpaqueOriginCreator creates opaque origins with specified nonces for
// sandboxed frames. This class should NOT be used for any other origin
// creation purposes.
class CONTENT_EXPORT SandboxedOpaqueOriginCreator {
 public:
  // Creates an opaque origin for sandboxed frames with the given nonce and
  // tuple. Only callable from RenderFrameHostImpl.
  static url::Origin CreateOriginForSandboxedFrame(
      base::PassKey<RenderFrameHostImpl>,
      const base::UnguessableToken& nonce,
      const url::SchemeHostPort& tuple);

  // Creates an opaque origin for sandboxed frames. Only callable from tests.
  static url::Origin CreateOriginForSandboxedFrameForTesting(
      base::PassKey<SandboxedOpaqueOriginCreatorTest>,
      const base::UnguessableToken& nonce,
      const url::SchemeHostPort& tuple);

  // Disable instantiation
  SandboxedOpaqueOriginCreator() = delete;
  SandboxedOpaqueOriginCreator(const SandboxedOpaqueOriginCreator&) = delete;
  SandboxedOpaqueOriginCreator& operator=(const SandboxedOpaqueOriginCreator&) =
      delete;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SANDBOXED_OPAQUE_ORIGIN_CREATOR_H_
