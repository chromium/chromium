// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MOJO_BINDER_POLICY_APPLIER_H_
#define CONTENT_BROWSER_MOJO_BINDER_POLICY_APPLIER_H_

#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
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
// See content/browser/preloading/prerender/README.md for more about capability
// control.
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

  // Returns the instance used by BrowserInterfaceBrokerImpl for preview mode.
  // This is used when a page is shown in preview mode.
  static std::unique_ptr<MojoBinderPolicyApplier> CreateForPreview(
      base::OnceCallback<void(const std::string& interface_name)>
          cancel_closure);

  // Disallows copy and move operations.
  MojoBinderPolicyApplier(const MojoBinderPolicyApplier& other) = delete;
  MojoBinderPolicyApplier& operator=(const MojoBinderPolicyApplier& other) =
      delete;
  MojoBinderPolicyApplier(MojoBinderPolicyApplier&&) = delete;
  MojoBinderPolicyApplier& operator=(MojoBinderPolicyApplier&&) = delete;

  // Applies `MojoBinderNonAssociatedPolicy` before binding a non-associated
  // interface.
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
  void ApplyPolicyToNonAssociatedBinder(const std::string& interface_name,
                                        base::OnceClosure binder_callback);

  // Applies `MojoBinderAssociatedPolicy` before binding an associated
  // interface. Note that this method only applies kCancel and kGrant to
  // associated intefaces, because messages sent over associated interfaces
  // cannot be deferred. See
  // https://chromium.googlesource.com/chromium/src/+/HEAD/mojo/public/cpp/bindings/README.md#Associated-Interfaces
  // for more information.
  // Runs the cancellation callback and returns false if kCancel is applied.
  // Otherwise returns true.
  bool ApplyPolicyToAssociatedBinder(const std::string& interface_name);

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
  MojoBinderNonAssociatedPolicy GetNonAssociatedMojoBinderPolicy(
      const std::string& interface_name) const;

  const MojoBinderNonAssociatedPolicy default_policy_ =
      MojoBinderNonAssociatedPolicy::kDefer;
  // Maps Mojo interface name to its policy.
  const raw_ref<const MojoBinderPolicyMapImpl> policy_map_;

  // Will be executed upon a request for a kCancel interface.
  base::OnceCallback<void(const std::string& interface_name)> cancel_callback_;
  Mode mode_ = Mode::kEnforce;

  // Stores binders which are delayed running.
  std::vector<base::OnceClosure> deferred_binders_;

  // Stores binders that can be used to send synchronous messages but
  // are delayed running.
  std::vector<base::OnceClosure> deferred_sync_binders_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_MOJO_BINDER_POLICY_APPLIER_H_
