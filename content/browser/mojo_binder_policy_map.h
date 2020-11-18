// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MOJO_BINDER_POLICY_MAP_H_
#define CONTENT_BROWSER_MOJO_BINDER_POLICY_MAP_H_

#include "base/containers/flat_map.h"

namespace content {

// MojoBinderPolicy specifies policies used by `MojoBinderPolicyMapApplier` for
// mojo capability control.
// See the comment in `MojoBinderPolicyApplier::ApplyPolicyToBinder()` for
// details.
enum class MojoBinderPolicy {
  // Run the binder registered for the requested interface as normal.
  kGrant,
  // Defer running the binder registered for the requested interface. Deferred
  // binders can be explicitly asked to run later.
  kDefer,
  // Used for interface requests that cannot be handled and should cause the
  // requesting context to be discarded (for example, cancel prerendering).
  kCancel,
  // The interface request is not expected. Kill the calling renderer.
  kUnexpected,
};

// Maps Mojo interface name to policy.
using MojoBinderPolicyMap = base::flat_map<std::string, MojoBinderPolicy>;

}  // namespace content

#endif  // CONTENT_BROWSER_MOJO_BINDER_POLICY_MAP_H_
