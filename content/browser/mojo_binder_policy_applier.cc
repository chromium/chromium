// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/mojo_binder_policy_applier.h"

#include "mojo/public/cpp/bindings/message.h"

namespace content {

MojoBinderPolicyApplier::MojoBinderPolicyApplier(
    const MojoBinderPolicyMapImpl* policy_map,
    base::OnceCallback<void(const std::string& interface_name)> cancel_callback)
    : policy_map_(*policy_map), cancel_callback_(std::move(cancel_callback)) {}

MojoBinderPolicyApplier::~MojoBinderPolicyApplier() = default;

// static
std::unique_ptr<MojoBinderPolicyApplier>
MojoBinderPolicyApplier::CreateForSameOriginPrerendering(
    base::OnceCallback<void(const std::string& interface_name)>
        cancel_callback) {
  return std::make_unique<MojoBinderPolicyApplier>(
      MojoBinderPolicyMapImpl::GetInstanceForSameOriginPrerendering(),
      std::move(cancel_callback));
}

void MojoBinderPolicyApplier::ApplyPolicyToBinder(
    const std::string& interface_name,
    base::OnceClosure binder_callback) {
  if (mode_ == Mode::kGrantAll) {
    std::move(binder_callback).Run();
    return;
  }
  const MojoBinderPolicy policy = GetMojoBinderPolicy(interface_name);

  // Run in the kPrepareToGrantAll mode before the renderer sends back a
  // DidCommitActivation. In this mode, MojoBinderPolicyApplier loosens
  // policies, but still defers binders to ensure that the renderer does not
  // receive unexpected messages before CommitActivation arrives.
  if (mode_ == Mode::kPrepareToGrantAll) {
    switch (policy) {
      case MojoBinderPolicy::kGrant:
      // Grant these two kinds of interfaces because:
      // - kCancel and kUnexpected interfaces may have sync methods, so grant
      // them to avoid deadlocks.
      // - Renderer might request these interfaces during the prerenderingchange
      // event, because from the page's point of view it is no longer
      // prerendering.
      case MojoBinderPolicy::kCancel:
      case MojoBinderPolicy::kUnexpected:
        std::move(binder_callback).Run();
        break;
      case MojoBinderPolicy::kDefer:
        deferred_binders_.push_back(std::move(binder_callback));
        break;
    }
    return;
  }

  DCHECK_EQ(mode_, Mode::kEnforce);
  switch (policy) {
    case MojoBinderPolicy::kGrant:
      std::move(binder_callback).Run();
      break;
    case MojoBinderPolicy::kCancel:
      if (cancel_callback_) {
        std::move(cancel_callback_).Run(interface_name);
      }
      break;
    case MojoBinderPolicy::kDefer:
      deferred_binders_.push_back(std::move(binder_callback));
      break;
    case MojoBinderPolicy::kUnexpected:
      mojo::ReportBadMessage("MBPA_BAD_INTERFACE: " + interface_name);
      if (cancel_callback_) {
        std::move(cancel_callback_).Run(interface_name);
      }
      break;
  }
}

void MojoBinderPolicyApplier::PrepareToGrantAll() {
  DCHECK_EQ(mode_, Mode::kEnforce);
  mode_ = Mode::kPrepareToGrantAll;
}

void MojoBinderPolicyApplier::GrantAll() {
  DCHECK_NE(mode_, Mode::kGrantAll);

  // GrantAll() should be called inside a Mojo message call stack, because it
  // binds deferred receivers by invoking
  // BrowserInterfaceBroker::BindInterface(), which assumes it is called within
  // a Mojo messaging call. See https://crbug.com/1217977 for more information.
  DCHECK(mojo::GetBadMessageCallback());
  mode_ = Mode::kGrantAll;

  // It's safe to iterate over `deferred_binders_` because no more callbacks
  // will be added to it once `grant_all_` is true."
  for (auto& deferred_binder : deferred_binders_)
    std::move(deferred_binder).Run();
  deferred_binders_.clear();
}

void MojoBinderPolicyApplier::DropDeferredBinders() {
  deferred_binders_.clear();
}

MojoBinderPolicy MojoBinderPolicyApplier::GetMojoBinderPolicy(
    const std::string& interface_name) const {
  return policy_map_.GetMojoBinderPolicy(interface_name, default_policy_);
}

}  // namespace content
