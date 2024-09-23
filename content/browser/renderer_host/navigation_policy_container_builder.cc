// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/navigation_policy_container_builder.h"

#include <utility>

#include "content/browser/renderer_host/frame_navigation_entry.h"
#include "content/browser/renderer_host/navigation_state_keep_alive.h"
#include "content/browser/renderer_host/policy_container_host.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "services/network/public/cpp/web_sandbox_flags.h"
#include "services/network/public/mojom/content_security_policy.mojom-forward.h"
#include "services/network/public/mojom/ip_address_space.mojom.h"
#include "services/network/public/mojom/web_sandbox_flags.mojom.h"

namespace content {
namespace {

// Returns a copy of |parent|'s policies, or nullopt if |parent| is nullptr.
std::unique_ptr<PolicyContainerPolicies> GetParentPolicies(
    RenderFrameHostImpl* parent) {
  if (!parent) {
    return nullptr;
  }

  return parent->policy_container_host()->policies().ClonePtr();
}

// Returns a copy of the navigation initiator's policies, if any.
//
// Must only be called on the browser's UI thread.
std::unique_ptr<PolicyContainerPolicies> GetInitiatorPolicies(
    const blink::LocalFrameToken* frame_token,
    int initiator_process_id,
    StoragePartitionImpl* storage_partition) {
  if (!frame_token) {
    return nullptr;
  }

  PolicyContainerHost* initiator_policy_container_host =
      RenderFrameHostImpl::GetPolicyContainerHost(
          frame_token, initiator_process_id, storage_partition);

  DCHECK(initiator_policy_container_host);
  if (!initiator_policy_container_host) {
    // Guard against wrong tokens being passed accidentally.
    return nullptr;
  }

  return initiator_policy_container_host->policies().ClonePtr();
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

  return policies->ClonePtr();
}

}  // namespace

NavigationPolicyContainerBuilder::NavigationPolicyContainerBuilder(
    RenderFrameHostImpl* parent,
    const blink::LocalFrameToken* initiator_frame_token,
    int initiator_process_id,
    StoragePartition* storage_partition,
    const FrameNavigationEntry* history_entry)
    : parent_policies_(GetParentPolicies(parent)),
      initiator_policies_(GetInitiatorPolicies(
          initiator_frame_token,
          initiator_process_id,
          static_cast<StoragePartitionImpl*>(storage_partition))),
      history_policies_(GetHistoryPolicies(history_entry)) {}

NavigationPolicyContainerBuilder::~NavigationPolicyContainerBuilder() = default;

const PolicyContainerPolicies*
NavigationPolicyContainerBuilder::InitiatorPolicies() const {
  return initiator_policies_.get();
}

const PolicyContainerPolicies*
NavigationPolicyContainerBuilder::ParentPolicies() const {
  return parent_policies_.get();
}

const PolicyContainerPolicies*
NavigationPolicyContainerBuilder::HistoryPolicies() const {
  return history_policies_.get();
}

void NavigationPolicyContainerBuilder::SetIPAddressSpace(
    network::mojom::IPAddressSpace address_space) {
  DCHECK(!HasComputedPolicies());
  delivered_policies_.ip_address_space = address_space;
}

void NavigationPolicyContainerBuilder::SetIsOriginPotentiallyTrustworthy(
    bool value) {
  DCHECK(!HasComputedPolicies());
  delivered_policies_.is_web_secure_context = value;
}

void NavigationPolicyContainerBuilder::SetAllowCrossOriginIsolation(
    bool value) {
  DCHECK(!HasComputedPolicies());
  delivered_policies_.allow_cross_origin_isolation = value;
}

void NavigationPolicyContainerBuilder::AddContentSecurityPolicy(
    network::mojom::ContentSecurityPolicyPtr policy) {
  DCHECK(!HasComputedPolicies());
  DCHECK(policy);

  delivered_policies_.content_security_policies.push_back(std::move(policy));
}

void NavigationPolicyContainerBuilder::AddContentSecurityPolicies(
    std::vector<network::mojom::ContentSecurityPolicyPtr> policies) {
  DCHECK(!HasComputedPolicies());

  delivered_policies_.AddContentSecurityPolicies(std::move(policies));
}

void NavigationPolicyContainerBuilder::SetCrossOriginOpenerPolicy(
    network::CrossOriginOpenerPolicy coop) {
  DCHECK(!HasComputedPolicies());

  delivered_policies_.cross_origin_opener_policy = coop;
}

void NavigationPolicyContainerBuilder::SetCrossOriginEmbedderPolicy(
    network::CrossOriginEmbedderPolicy coep) {
  DCHECK(!HasComputedPolicies());

  delivered_policies_.cross_origin_embedder_policy = coep;
}

void NavigationPolicyContainerBuilder::SetDocumentIsolationPolicy(
    const network::DocumentIsolationPolicy& dip) {
  DCHECK(!HasComputedPolicies());

  delivered_policies_.document_isolation_policy = dip;
}

const PolicyContainerPolicies&
NavigationPolicyContainerBuilder::DeliveredPoliciesForTesting() const {
  DCHECK(!HasComputedPolicies());

  return delivered_policies_;
}

void NavigationPolicyContainerBuilder::ComputePoliciesForError() {
  // The decision to commit an error page can happen after receiving the
  // response for a regular document. It overrides any previous attempt to
  // |ComputePolicies()|.
  host_ = nullptr;

  DCHECK(!HasComputedPolicies());

  // TODO(crbug.com/40747546): We should enforce strict policies on error
  // pages.
  PolicyContainerPolicies policies;

  // We commit error pages with the same address space as the underlying page,
  // so that auto-reloading error pages does not show up as a private network
  // request (from the unknown/public address space to private). See also
  // crbug.com/1180140.
  policies.ip_address_space = delivered_policies_.ip_address_space;

  SetFinalPolicies(std::move(policies));

  DCHECK(HasComputedPolicies());
}

void NavigationPolicyContainerBuilder::ComputeIsWebSecureContext() {
  DCHECK(!HasComputedPolicies());

  if (!parent_policies_) {
    // No parent. Only the trustworthiness of the origin matters.
    return;
  }

  // The child can only be a secure context if the parent is too.
  delivered_policies_.is_web_secure_context &=
      parent_policies_->is_web_secure_context;
}

void NavigationPolicyContainerBuilder::ComputeSandboxFlags(
    bool is_inside_mhtml,
    network::mojom::WebSandboxFlags frame_sandbox_flags,
    PolicyContainerPolicies& policies) {
  DCHECK(!HasComputedPolicies());

  auto sandbox_flags_to_commit = frame_sandbox_flags;

  // The document can also restrict sandbox further, via its CSP.
  for (const auto& csp : policies.content_security_policies) {
    sandbox_flags_to_commit |= csp->sandbox;
  }

  // The URL of a document loaded from a MHTML archive is controlled by the
  // Content-Location header. This can be set to an arbitrary URL. This is
  // potentially dangerous. For this reason we force the document to be
  // sandboxed, providing exceptions only for creating new windows. This
  // includes disallowing javascript and using an opaque origin.
  if (is_inside_mhtml) {
    sandbox_flags_to_commit |= ~network::mojom::WebSandboxFlags::kPopups &
                               ~network::mojom::WebSandboxFlags::
                                   kPropagatesToAuxiliaryBrowsingContexts;
  }

  policies.sandbox_flags = sandbox_flags_to_commit;
}

void NavigationPolicyContainerBuilder::IncorporateDeliveredPolicies(
    const GURL& url,
    PolicyContainerPolicies& policies) {
  // Delivered content security policies must be appended.
  policies.AddContentSecurityPolicies(
      mojo::Clone(delivered_policies_.content_security_policies));

  // The delivered IP address space (if any) overrides the IP address space.
  if (delivered_policies_.ip_address_space !=
      network::mojom::IPAddressSpace::kUnknown) {
    policies.ip_address_space = delivered_policies_.ip_address_space;
  }
}

PolicyContainerPolicies
NavigationPolicyContainerBuilder::ComputeInheritedPolicies(const GURL& url) {
  DCHECK(url.SchemeIsLocal()) << url << " should not inherit policies";

  if (url.IsAboutSrcdoc()) {
    DCHECK(parent_policies_)
        << "About:srcdoc documents should always have a parent frame.";
    return parent_policies_->Clone();
  }

  if (initiator_policies_) {
    return initiator_policies_->Clone();
  }

  return PolicyContainerPolicies();
}

PolicyContainerPolicies NavigationPolicyContainerBuilder::ComputeFinalPolicies(
    const GURL& url,
    bool is_inside_mhtml,
    network::mojom::WebSandboxFlags frame_sandbox_flags,
    bool is_credentialless) {
  PolicyContainerPolicies policies;

  // Policies are either inherited from another document for local scheme, or
  // directly set from the delivered response.
  if (!url.SchemeIsLocal()) {
    policies = delivered_policies_.Clone();
  } else if (history_policies_) {
    // For a local scheme, history policies should not incorporate delivered
    // ones as this may lead to duplication of some policies already stored in
    // history. For example, consider the following HTML:
    //    <iframe src="about:blank" csp="something">
    // This will store CSP: something in history. The next time we have a
    // history navigation we will have CSP: something twice.
    policies = history_policies_->Clone();
  } else {
    policies = ComputeInheritedPolicies(url);
    IncorporateDeliveredPolicies(url, policies);
  }

  // `can_navigate_top_without_user_gesture` is inherited from the parent.
  // Later in `NavigationRequest::CommitNavigation()` it will either be made
  // less strict for same-origin navigations, or stricter for cross-origin
  // navigations that do not explicitly allow top-level navigation without user
  // gesture.
  policies.can_navigate_top_without_user_gesture =
      parent_policies_ ? parent_policies_->can_navigate_top_without_user_gesture
                       : true;

  ComputeSandboxFlags(is_inside_mhtml, frame_sandbox_flags, policies);
  policies.is_credentialless = is_credentialless;
  return policies;
}

void NavigationPolicyContainerBuilder::ComputePolicies(
    const GURL& url,
    bool is_inside_mhtml,
    network::mojom::WebSandboxFlags frame_sandbox_flags,
    bool is_credentialless) {
  DCHECK(!HasComputedPolicies());
  ComputeIsWebSecureContext();
  SetFinalPolicies(ComputeFinalPolicies(
      url, is_inside_mhtml, frame_sandbox_flags, is_credentialless));
}

bool NavigationPolicyContainerBuilder::HasComputedPolicies() const {
  return host_ != nullptr;
}

void NavigationPolicyContainerBuilder::SetAllowTopNavigationWithoutUserGesture(
    bool allow_top) {
  host_->SetCanNavigateTopWithoutUserGesture(allow_top);
}

void NavigationPolicyContainerBuilder::SetFinalPolicies(
    PolicyContainerPolicies policies) {
  DCHECK(!HasComputedPolicies());

  host_ = base::MakeRefCounted<PolicyContainerHost>(std::move(policies));
}

const PolicyContainerPolicies& NavigationPolicyContainerBuilder::FinalPolicies()
    const {
  DCHECK(HasComputedPolicies());

  return host_->policies();
}

blink::mojom::PolicyContainerPtr
NavigationPolicyContainerBuilder::CreatePolicyContainerForBlink() {
  DCHECK(HasComputedPolicies());

  return host_->CreatePolicyContainerForBlink();
}

scoped_refptr<PolicyContainerHost>
NavigationPolicyContainerBuilder::GetPolicyContainerHost() {
  DCHECK(HasComputedPolicies());
  CHECK(host_);

  return host_;
}

scoped_refptr<PolicyContainerHost>
NavigationPolicyContainerBuilder::TakePolicyContainerHost() && {
  DCHECK(HasComputedPolicies());

  return std::move(host_);
}

void NavigationPolicyContainerBuilder::ResetForCrossDocumentRestart() {
  host_ = nullptr;
  delivered_policies_ = PolicyContainerPolicies();
}

}  // namespace content
