// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_MOJO_BINDER_POLICY_MAP_H_
#define CONTENT_PUBLIC_BROWSER_MOJO_BINDER_POLICY_MAP_H_

#include "base/containers/flat_map.h"
#include "base/strings/string_piece_forward.h"
#include "content/common/content_export.h"

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

// Used by content/ layer to manage interfaces' binding policies. Embedders can
// set their own policies via this interface.
// TODO(https://crbug.com/1157334): Consider integrating it with
// mojo::BinderMap.
class CONTENT_EXPORT MojoBinderPolicyMap {
 public:
  MojoBinderPolicyMap() = default;
  virtual ~MojoBinderPolicyMap() = default;

  // Called by embedders to set their binder policies.
  template <typename Interface>
  void SetPolicy(MojoBinderPolicy policy) {
    SetPolicyByName(Interface::Name_, policy);
  }

 private:
  virtual void SetPolicyByName(const base::StringPiece& name,
                               MojoBinderPolicy policy) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_MOJO_BINDER_POLICY_MAP_H_
