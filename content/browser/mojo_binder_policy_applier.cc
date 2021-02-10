// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/mojo_binder_policy_applier.h"

namespace content {

MojoBinderPolicyApplier::MojoBinderPolicyApplier(
    const MojoBinderPolicyMapImpl* policy_map,
    base::OnceClosure cancel_closure)
    : policy_map_(*policy_map), cancel_closure_(std::move(cancel_closure)) {}

MojoBinderPolicyApplier::~MojoBinderPolicyApplier() = default;

// static
std::unique_ptr<MojoBinderPolicyApplier>
MojoBinderPolicyApplier::CreateForPrerendering(
    base::OnceClosure cancel_closure) {
  return std::make_unique<MojoBinderPolicyApplier>(
      MojoBinderPolicyMapImpl::GetInstanceForPrerendering(),
      std::move(cancel_closure));
}

void MojoBinderPolicyApplier::ApplyPolicyToBinder(
    const std::string& interface_name,
    base::OnceClosure binder_callback) {
  if (grant_all_) {
    std::move(binder_callback).Run();
    return;
  }
  const MojoBinderPolicy policy = GetMojoBinderPolicy(interface_name);
  switch (policy) {
    case MojoBinderPolicy::kGrant:
      std::move(binder_callback).Run();
      break;
    case MojoBinderPolicy::kCancel:
      if (cancel_closure_)
        std::move(cancel_closure_).Run();
      break;
    case MojoBinderPolicy::kDefer:
      deferred_binders_.push_back(std::move(binder_callback));
      break;
    case MojoBinderPolicy::kUnexpected:
      // TODO(crbug.com/1141364): Report a metric to understand the unexpected
      // case.
      break;
  }
}

void MojoBinderPolicyApplier::GrantAll() {
  DCHECK(!grant_all_);
  grant_all_ = true;
  // It's safe to iterate over `deferred_binders_` because no more callbacks
  // will be added to it once `grant_all_` is true."
  for (auto& deferred_binder : deferred_binders_)
    std::move(deferred_binder).Run();
  deferred_binders_.clear();
}

MojoBinderPolicy MojoBinderPolicyApplier::GetMojoBinderPolicy(
    const std::string& interface_name) const {
  return policy_map_.GetMojoBinderPolicy(interface_name, default_policy_);
}

}  // namespace content
