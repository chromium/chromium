// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/policy_container_navigation_bundle.h"

#include <utility>

#include "content/browser/renderer_host/frame_navigation_entry.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"

namespace content {
namespace {

// Returns whether |url| has a local scheme - i.e. a document that commits with
// |url| should inherit its policies from the initiator or the parent frame.
//
// If |url| is not `about:srcdoc` and this function returns true, then the
// document should inherit its policies from the initiator.
bool HasLocalScheme(const GURL& url) {
  return url.SchemeIs(url::kAboutScheme) || url.SchemeIs(url::kDataScheme) ||
         url.SchemeIs(url::kBlobScheme) || url.SchemeIs(url::kFileSystemScheme);
}

// Returns a copy of |parent|'s policies, or nullopt if |parent| is nullptr.
std::unique_ptr<PolicyContainerPolicies> GetParentPolicies(
    RenderFrameHostImpl* parent) {
  if (!parent) {
    return nullptr;
  }

  return std::make_unique<PolicyContainerPolicies>(
      parent->policy_container_host()->policies());
}

// Returns a copy of the navigation initiator's policies, if any.
//
// Must only be called on the browser's UI thread.
std::unique_ptr<PolicyContainerPolicies> GetInitiatorPolicies(
    const blink::LocalFrameToken* frame_token) {
  if (!frame_token) {
    return nullptr;
  }

  // We use PolicyContainerHost::FromFrameToken directly since this will
  // retrieve the PolicyContainerHost of the initiator RenderFrameHost even if
  // the RenderFrameHost has already been deleted.
  PolicyContainerHost* initiator_policy_container_host =
      PolicyContainerHost::FromFrameToken(*frame_token);
  DCHECK(initiator_policy_container_host);
  if (!initiator_policy_container_host) {
    // Guard against wrong tokens being passed accidentally.
    return nullptr;
  }

  return std::make_unique<PolicyContainerPolicies>(
      initiator_policy_container_host->policies());
}

// Returns a copy of the given history |entry|'s policies, if any.
std::unique_ptr<PolicyContainerPolicies> GetHistoryPolicies(
    const FrameNavigationEntry* entry) {
  if (!entry) {
    return nullptr;
  }

  const PolicyContainerPolicies* policies = entry->policy_container_policies();
  if (!policies) {
    return nullptr;
  }

  return std::make_unique<PolicyContainerPolicies>(*policies);
}

}  // namespace

PolicyContainerNavigationBundle::PolicyContainerNavigationBundle(
    RenderFrameHostImpl* parent,
    const blink::LocalFrameToken* initiator_frame_token,
    const FrameNavigationEntry* history_entry)
    : parent_policies_(GetParentPolicies(parent)),
      initiator_policies_(GetInitiatorPolicies(initiator_frame_token)),
      history_policies_(GetHistoryPolicies(history_entry)) {}

PolicyContainerNavigationBundle::~PolicyContainerNavigationBundle() = default;

const PolicyContainerPolicies*
PolicyContainerNavigationBundle::InitiatorPolicies() const {
  return initiator_policies_.get();
}

const PolicyContainerPolicies* PolicyContainerNavigationBundle::ParentPolicies()
    const {
  return parent_policies_.get();
}

const PolicyContainerPolicies*
PolicyContainerNavigationBundle::HistoryPolicies() const {
  return history_policies_.get();
}

void PolicyContainerNavigationBundle::SetIPAddressSpace(
    network::mojom::IPAddressSpace address_space) {
  DCHECK(!IsFinalized());

  delivered_policies_.ip_address_space = address_space;
}

const PolicyContainerPolicies&
PolicyContainerNavigationBundle::DeliveredPolicies() const {
  return delivered_policies_;
}

void PolicyContainerNavigationBundle::FinalizePoliciesForError() {
  DCHECK(!IsFinalized());

  // TODO(https://crbug.com/1175787): We should enforce strict policies on error
  // pages.
  PolicyContainerPolicies policies;

  // We commit error pages with the same address space as the underlying page,
  // so that auto-reloading error pages does not show up as a private network
  // request (from the unknown/public address space to private). See also
  // crbug.com/1180140.
  policies.ip_address_space = delivered_policies_.ip_address_space;

  SetFinalPolicies(policies);
}

void PolicyContainerNavigationBundle::FinalizePolicies(const GURL& url) {
  DCHECK(!IsFinalized());

  const PolicyContainerPolicies* override_policies =
      ComputeFinalPoliciesOverride(url);
  if (override_policies) {
    SetFinalPolicies(*override_policies);
    return;
  }

  SetFinalPolicies(delivered_policies_);
}

bool PolicyContainerNavigationBundle::IsFinalized() const {
  return host_ != nullptr;
}

const PolicyContainerPolicies*
PolicyContainerNavigationBundle::ComputeFinalPoliciesOverride(
    const GURL& url) const {
  DCHECK(!IsFinalized());

  if (history_policies_) {
    DCHECK(HasLocalScheme(url))
        << "Document is restoring policies from history for non-local scheme: "
        << url;
    return history_policies_.get();
  }

  if (url.IsAboutSrcdoc()) {
    DCHECK(parent_policies_)
        << "About:srcdoc documents should always have a parent frame.";
    return ParentPolicies();
  }

  if (HasLocalScheme(url)) {
    return InitiatorPolicies();
  }

  return nullptr;
}

void PolicyContainerNavigationBundle::SetFinalPolicies(
    const PolicyContainerPolicies& policies) {
  DCHECK(!IsFinalized());

  host_ = base::MakeRefCounted<PolicyContainerHost>(policies);
}

const PolicyContainerPolicies& PolicyContainerNavigationBundle::FinalPolicies()
    const {
  DCHECK(IsFinalized());

  return host_->policies();
}

blink::mojom::PolicyContainerPtr
PolicyContainerNavigationBundle::CreatePolicyContainerForBlink() {
  DCHECK(IsFinalized());

  return host_->CreatePolicyContainerForBlink();
}

scoped_refptr<PolicyContainerHost>
PolicyContainerNavigationBundle::TakePolicyContainerHost() && {
  DCHECK(IsFinalized());

  return std::move(host_);
}

}  // namespace content
