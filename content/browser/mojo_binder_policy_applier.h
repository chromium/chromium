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
  enum class Mode {
    // In the kEnforce mode, MojoBinderPolicyApplier processes binding requests
    // strictly according to the pre-set policies.
    kEnforce,
    // If the page is about to activate, MojoBinderPolicyApplier will switch to
    // the kPrepareToGrantAll mode, and all non-kGrant binders will be
    // deferred.
    kPrepareToGrantAll,
    // In the kGrantAll mode, MojoBinderPolicyApplier grants all binding
    // requests regardless of their policies.
    kGrantAll,
  };

  // `policy_map` must outlive `this` and must not be null.
  // `cancel_callback` will be executed when ApplyPolicyToBinder() processes a
  // kCancel interface.
  MojoBinderPolicyApplier(
      const MojoBinderPolicyMapImpl* policy_map,
      base::OnceCallback<void(const std::string& interface_name)>
          cancel_callback);
  ~MojoBinderPolicyApplier();

  // Returns the instance used by BrowserInterfaceBrokerImpl for same-origin
  // prerendering pages. This is used when the prerendered page and the page
  // that triggered the prerendering are same origin.
  static std::unique_ptr<MojoBinderPolicyApplier>
  CreateForSameOriginPrerendering(
      base::OnceCallback<void(const std::string& interface_name)>
          cancel_closure);

  // Disallows copy and move operations.
  MojoBinderPolicyApplier(const MojoBinderPolicyApplier& other) = delete;
  MojoBinderPolicyApplier& operator=(const MojoBinderPolicyApplier& other) =
      delete;
  MojoBinderPolicyApplier(MojoBinderPolicyApplier&&) = delete;
  MojoBinderPolicyApplier& operator=(MojoBinderPolicyApplier&&) = delete;

  // Applies `MojoBinderPolicy` before binding an interface.
  // - In kEnforce mode:
  //   - kGrant: Runs `binder_callback` immediately.
  //   - kDefer: Saves `binder_callback` and runs it when GrantAll() is called.
  //   - kCancel: Drops `binder_callback` and runs `cancel_callback_`.
  //   - kUnexpected: Unimplemented now.
  // - In the kPrepareToGrantAll mode:
  //   - kGrant: Runs `binder_callback` immediately.
  //   - kDefer, kCancel and kUnexpected: Saves `binder_callback` and runs it
  //   when GrantAll() is called.
  // - In the kGrantAll mode: this always runs the callback immediately.
  void ApplyPolicyToBinder(const std::string& interface_name,
                           base::OnceClosure binder_callback);
  // Switches this to the kPrepareToGrantAll mode.
  void PrepareToGrantAll();
  // Runs all deferred binders and runs binder callbacks for all subsequent
  // requests, i.e., it stops applying the policies.
  void GrantAll();
  // Deletes all deferred binders without running them.
  void DropDeferredBinders();

 private:
  friend class MojoBinderPolicyApplierTest;

  // Gets the corresponding policy of the given mojo interface name.
  MojoBinderPolicy GetMojoBinderPolicy(const std::string& interface_name) const;

  const MojoBinderPolicy default_policy_ = MojoBinderPolicy::kDefer;
  // Maps Mojo interface name to its policy.
  const MojoBinderPolicyMapImpl& policy_map_;
  // Will be executed upon a request for a kCancel interface.
  base::OnceCallback<void(const std::string& interface_name)> cancel_callback_;
  Mode mode_ = Mode::kEnforce;
  // Stores binders which are delayed running.
  std::vector<base::OnceClosure> deferred_binders_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_MOJO_BINDER_POLICY_APPLIER_H_
