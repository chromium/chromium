// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_MOJO_BINDER_POLICY_MAP_H_
#define CONTENT_PUBLIC_BROWSER_MOJO_BINDER_POLICY_MAP_H_

#include <string_view>

#include "base/check_op.h"
#include "content/common/content_export.h"

namespace content {

// MojoBinderNonAssociatedPolicy specifies policies for non-associated
// interfaces. It is used by `MojoBinderPolicyMapApplier` for Mojo
// capability control.
// See the comment in
// `MojoBinderPolicyApplier::ApplyPolicyToNonAssociatedBinder()` for details.
enum class MojoBinderNonAssociatedPolicy {
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

// MojoBinderAssociatedPolicy specifies policies for channel-associated
// interfaces. It is used by `MojoBinderPolicyMapApplier` for Mojo capability
// control. See the comment in
// `MojoBinderPolicyApplier::ApplyPolicyToAssociatedBinder()` for details.
enum class MojoBinderAssociatedPolicy {
  // Run the binder registered for the requested interface as normal.
  kGrant,
  // Used for interface requests that cannot be handled and should cause the
  // requesting context to be discarded (for example, cancel prerendering).
  kCancel,
  // The interface request is not expected. Kill the calling renderer.
  kUnexpected,
};

// Used by content/ layer to manage interfaces' binding policies. Embedders can
// set their own policies via this interface.
// TODO(crbug.com/40160797): Consider integrating it with
// mojo::BinderMap.
class CONTENT_EXPORT MojoBinderPolicyMap {
 public:
  MojoBinderPolicyMap() = default;
  virtual ~MojoBinderPolicyMap() = default;

  // Called by embedders to set their binder policies for channel-associated
  // interfaces.
  template <typename Interface>
  void SetAssociatedPolicy(MojoBinderAssociatedPolicy policy) {
    SetPolicyByName(Interface::Name_, policy);
  }

  // Called by embedders to set their binder policies for non-associated
  // interfaces.
  template <typename Interface>
  void SetNonAssociatedPolicy(MojoBinderNonAssociatedPolicy policy) {
    SetPolicyByName(Interface::Name_, policy);
  }

 private:
  virtual void SetPolicyByName(const std::string_view& name,
                               MojoBinderAssociatedPolicy policy) = 0;

  virtual void SetPolicyByName(const std::string_view& name,
                               MojoBinderNonAssociatedPolicy policy) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_MOJO_BINDER_POLICY_MAP_H_
