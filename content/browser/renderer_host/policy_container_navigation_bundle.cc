// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/policy_container_navigation_bundle.h"

#include <utility>

#include "content/browser/renderer_host/frame_navigation_entry.h"
#include "content/browser/renderer_host/policy_container_host.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "services/network/public/cpp/web_sandbox_flags.h"
#include "services/network/public/mojom/content_security_policy.mojom-forward.h"
#include "services/network/public/mojom/ip_address_space.mojom-shared.h"
#include "services/network/public/mojom/web_sandbox_flags.mojom.h"

namespace content {
namespace {

// Returns a copy of |parent|'s policies, or nullopt if |parent| is nullptr.
std::unique_ptr<PolicyContainerPolicies> GetParentPolicies(
    RenderFrameHostImpl* parent) {
  if (!parent) {
    return nullptr;
  }

  return parent->policy_container_host()->policies().Clone();
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

  return initiator_policy_container_host->policies().Clone();
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

  return policies->Clone();
}

}  // namespace

PolicyContainerNavigationBundle::PolicyContainerNavigationBundle(
    RenderFrameHostImpl* parent,
    const blink::LocalFrameToken* initiator_frame_token,
    const FrameNavigationEntry* history_entry)
    : parent_policies_(GetParentPolicies(parent)),
      initiator_policies_(GetInitiatorPolicies(initiator_frame_token)),
      history_policies_(GetHistoryPolicies(history_entry)),
      delivered_policies_(std::make_unique<PolicyContainerPolicies>()) {}

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
  DCHECK(!HasComputedPolicies());
  delivered_policies_->ip_address_space = address_space;
}

void PolicyContainerNavigationBundle::SetIsOriginPotentiallyTrustworthy(
    bool value) {
  DCHECK(!HasComputedPolicies());
  delivered_policies_->is_web_secure_context = value;
}

void PolicyContainerNavigationBundle::AddContentSecurityPolicy(
    network::mojom::ContentSecurityPolicyPtr policy) {
  DCHECK(!HasComputedPolicies());
  DCHECK(policy);

  delivered_policies_->content_security_policies.push_back(std::move(policy));
}

void PolicyContainerNavigationBundle::AddContentSecurityPolicies(
    std::vector<network::mojom::ContentSecurityPolicyPtr> policies) {
  DCHECK(!HasComputedPolicies());

  delivered_policies_->AddContentSecurityPolicies(std::move(policies));
}

void PolicyContainerNavigationBundle::SetCrossOriginOpenerPolicy(
    network::CrossOriginOpenerPolicy coop) {
  DCHECK(!HasComputedPolicies());

  delivered_policies_->cross_origin_opener_policy = coop;
}

void PolicyContainerNavigationBundle::SetCrossOriginEmbedderPolicy(
    network::CrossOriginEmbedderPolicy coep) {
  DCHECK(!HasComputedPolicies());

  delivered_policies_->cross_origin_embedder_policy = coep;
}

const PolicyContainerPolicies&
PolicyContainerNavigationBundle::DeliveredPoliciesForTesting() const {
  DCHECK(!HasComputedPolicies());

  return *delivered_policies_;
}

void PolicyContainerNavigationBundle::ComputePoliciesForError(
    bool is_inside_mhtml,
    network::mojom::WebSandboxFlags frame_sandbox_flags) {
  // The decision to commit an error page can happen after receiving the
  // response for a regular document. It overrides any previous attempt to
  // |ComputePolicies()|.
  host_ = nullptr;

  DCHECK(!HasComputedPolicies());

  // TODO(https://crbug.com/1175787): We should enforce strict policies on error
  // pages.
  auto policies = std::make_unique<PolicyContainerPolicies>();

  // We commit error pages with the same address space as the underlying page,
  // so that auto-reloading error pages does not show up as a private network
  // request (from the unknown/public address space to private). See also
  // crbug.com/1180140.
  policies->ip_address_space = delivered_policies_->ip_address_space;

  ComputeSandboxFlags(is_inside_mhtml, frame_sandbox_flags, policies.get());

  SetFinalPolicies(std::move(policies));

  DCHECK(HasComputedPolicies());
}

void PolicyContainerNavigationBundle::ComputeIsWebSecureContext() {
  DCHECK(!HasComputedPolicies());

  if (!parent_policies_) {
    // No parent. Only the trustworthiness of the origin matters.
    return;
  }

  // The child can only be a secure context if the parent is too.
  delivered_policies_->is_web_secure_context &=
      parent_policies_->is_web_secure_context;
}

void PolicyContainerNavigationBundle::ComputeSandboxFlags(
    bool is_inside_mhtml,
    network::mojom::WebSandboxFlags frame_sandbox_flags,
    PolicyContainerPolicies* policies) {
  DCHECK(!HasComputedPolicies());

  auto sandbox_flags_to_commit = frame_sandbox_flags;

  // The document can also restrict sandbox further, via its CSP.
  for (const auto& csp : policies->content_security_policies) {
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

  policies->sandbox_flags = sandbox_flags_to_commit;
}

std::unique_ptr<PolicyContainerPolicies>
PolicyContainerNavigationBundle::IncorporateDeliveredPolicies(
    const GURL& url,
    std::unique_ptr<PolicyContainerPolicies> policies) {
  // Delivered content security policies must be appended.
  policies->AddContentSecurityPolicies(
      mojo::Clone(delivered_policies_->content_security_policies));

  // The delivered IP address space (if any) overrides the IP address space.
  if (delivered_policies_->ip_address_space !=
      network::mojom::IPAddressSpace::kUnknown) {
    policies->ip_address_space = delivered_policies_->ip_address_space;
  }

  return policies;
}

std::unique_ptr<PolicyContainerPolicies>
PolicyContainerNavigationBundle::ComputeInheritedPolicies(const GURL& url) {
  DCHECK(url.SchemeIsLocal()) << url << " should not inherit policies";

  if (url.IsAboutSrcdoc()) {
    DCHECK(parent_policies_)
        << "About:srcdoc documents should always have a parent frame.";
    return parent_policies_->Clone();
  }

  if (initiator_policies_) {
    return initiator_policies_->Clone();
  }

  return std::make_unique<PolicyContainerPolicies>();
}

std::unique_ptr<PolicyContainerPolicies>
PolicyContainerNavigationBundle::ComputeFinalPolicies(
    const GURL& url,
    bool is_inside_mhtml,
    network::mojom::WebSandboxFlags frame_sandbox_flags) {
  std::unique_ptr<PolicyContainerPolicies> policies;

  // Policies are either inherited from another document for local scheme, or
  // directly set from the delivered response.
  if (!url.SchemeIsLocal()) {
    policies = delivered_policies_->Clone();
  } else if (history_policies_) {
    // For a local scheme, history policies should not incorporate delivered
    // ones as this may lead to duplication of some policies already stored in
    // history. For example, consider the following HTML:
    //    <iframe src="about:blank" csp="something">
    // This will store CSP: something in history. The next time we have a
    // history navigation we will have CSP: something twice.
    policies = history_policies_->Clone();
  } else {
    policies = IncorporateDeliveredPolicies(url, ComputeInheritedPolicies(url));
  }

  ComputeSandboxFlags(is_inside_mhtml, frame_sandbox_flags, policies.get());
  return policies;
}

void PolicyContainerNavigationBundle::ComputePolicies(
    const GURL& url,
    bool is_inside_mhtml,
    network::mojom::WebSandboxFlags frame_sandbox_flags) {
  DCHECK(!HasComputedPolicies());
  ComputeIsWebSecureContext();
  SetFinalPolicies(
      ComputeFinalPolicies(url, is_inside_mhtml, frame_sandbox_flags));
}

bool PolicyContainerNavigationBundle::HasComputedPolicies() const {
  return host_ != nullptr;
}

void PolicyContainerNavigationBundle::SetFinalPolicies(
    std::unique_ptr<PolicyContainerPolicies> policies) {
  DCHECK(!HasComputedPolicies());

  host_ = base::MakeRefCounted<PolicyContainerHost>(std::move(policies));
}

const PolicyContainerPolicies& PolicyContainerNavigationBundle::FinalPolicies()
    const {
  DCHECK(HasComputedPolicies());

  return host_->policies();
}

blink::mojom::PolicyContainerPtr
PolicyContainerNavigationBundle::CreatePolicyContainerForBlink() {
  DCHECK(HasComputedPolicies());

  return host_->CreatePolicyContainerForBlink();
}

scoped_refptr<PolicyContainerHost>
PolicyContainerNavigationBundle::TakePolicyContainerHost() && {
  DCHECK(HasComputedPolicies());

  return std::move(host_);
}

void PolicyContainerNavigationBundle::ResetForCrossDocumentRestart() {
  host_ = nullptr;
  delivered_policies_ = std::make_unique<PolicyContainerPolicies>();
}

}  // namespace content
