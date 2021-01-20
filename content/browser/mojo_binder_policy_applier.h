// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MOJO_BINDER_POLICY_APPLIER_H_
#define CONTENT_BROWSER_MOJO_BINDER_POLICY_APPLIER_H_

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "content/browser/mojo_binder_policy_map_impl.h"
#include "content/common/content_export.h"

namespace content {

// MojoBinderPolicyApplier is a helper class for `BrowserInterfaceBrokerImpl`
// which allows control over when to run the binder registered for a
// requested interface. This is useful in cases like prerendering pages, where
// it can be desirable to defer binding until the page is activated, or take
// other actions.
//
// The action to take for each interface is specified in the given
// `MojoBinderPolicyMap`, and kDefer is used when no policy is specified.
//
// See content/browser/prerender/README.md for more about capability control.
class CONTENT_EXPORT MojoBinderPolicyApplier {
 public:
  // `policy_map` must outlive `this` and must not be null.
  // `cancel_closure` will be executed when ApplyPolicyToBinder() processes a
  // kCancel interface.
  MojoBinderPolicyApplier(const MojoBinderPolicyMapImpl* policy_map,
                          base::OnceClosure cancel_closure);
  ~MojoBinderPolicyApplier();

  // Returns the instance used by BrowserInterfaceBrokerImpl for pages that are
  // prerendering.
  static std::unique_ptr<MojoBinderPolicyApplier> CreateForPrerendering(
      base::OnceClosure cancel_closure);

  // Disallows copy and move operations.
  MojoBinderPolicyApplier(const MojoBinderPolicyApplier& other) = delete;
  MojoBinderPolicyApplier& operator=(const MojoBinderPolicyApplier& other) =
      delete;
  MojoBinderPolicyApplier(MojoBinderPolicyApplier&&) = delete;
  MojoBinderPolicyApplier& operator=(MojoBinderPolicyApplier&&) = delete;

  // Applies `MojoBinderPolicy` before binding an interface.
  // - kGrant: Runs `binder_callback` immediately.
  // - kDefer: Saves `binder_callback` and runs it when GrantAll() is called.
  // - kCancel: Drops `binder_callback` and runs `cancel_closure_`.
  // - kUnexpected: Unimplemented now.
  // If GrantAll() was already called, this always runs the callback
  // immediately.
  void ApplyPolicyToBinder(const std::string& interface_name,
                           base::OnceClosure binder_callback);
  // Runs all deferred binders and runs binder callbacks for all subsequent
  // requests, i.e., it stops applying the policies.
  void GrantAll();

 private:
  // Gets the corresponding policy of the given mojo interface name.
  MojoBinderPolicy GetMojoBinderPolicy(const std::string& interface_name) const;

  const MojoBinderPolicy default_policy_ = MojoBinderPolicy::kDefer;
  // Maps Mojo interface name to its policy.
  const MojoBinderPolicyMapImpl& policy_map_;
  // Will be executed upon a request for a kCancel interface.
  base::OnceClosure cancel_closure_;
  // Indicates if MojoBinderPolicyApplier grants all binding requests regardless
  // of their policies.
  bool grant_all_ = false;
  // Stores binders which are delayed running.
  std::vector<base::OnceClosure> deferred_binders_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_MOJO_BINDER_POLICY_APPLIER_H_
