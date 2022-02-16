// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_POLICY_CONTAINER_NAVIGATION_BUNDLE_H_
#define CONTENT_BROWSER_RENDERER_HOST_POLICY_CONTAINER_NAVIGATION_BUNDLE_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/unguessable_token.h"
#include "content/browser/renderer_host/policy_container_host.h"
#include "content/common/content_export.h"
#include "services/network/public/mojom/ip_address_space.mojom-shared.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/frame/policy_container.mojom.h"
#include "url/gurl.h"

namespace content {

class FrameNavigationEntry;
class RenderFrameHostImpl;

// Helper for NavigationRequest. Keeps track of a few important sets of policies
// (that of the parent document, of the navigation initiator, etc.) and computes
// the policies of the new document being navigated to.
//
// Instances of this class live in NavigationRequest. They are instantiated when
// the NavigationRequest is constructed, and destroyed at commit time.
//
// Setters can be called as the navigation progresses to record interesting
// properties for later.
//
// When the potential response to commit is known, |ComputePolicies()| can be
// called to set the final polices of the new document and create a new policy
// container host.
// For error documents, |ComputePoliciesForError()| should be used instead. It
// can also be called after |ComputePolicies()| in some cases when the error is
// only detected after receiving a response
//
// At commit time, |TakePolicyContainerHost()| can be called to transfer
// ownership of the policy container host to the target RenderFrameHostImpl.
class CONTENT_EXPORT PolicyContainerNavigationBundle {
 public:
  // All arguments may be nullptr and need only outlive this call.
  //
  // If |parent| is not nullptr, its policies are copied.
  // If |initiator_frame_token| is not nullptr and maps to a
  // PolicyContainerHost, then its policies are copied.
  // If |history_entry| is not nullptr and contains policies, those are copied.
  //
  // This must only be called on the browser's UI thread.
  PolicyContainerNavigationBundle(
      RenderFrameHostImpl* parent,
      const blink::LocalFrameToken* initiator_frame_token,
      const FrameNavigationEntry* history_entry);

  ~PolicyContainerNavigationBundle();

  // Instances of this class are neither copyable nor movable.
  PolicyContainerNavigationBundle(const PolicyContainerNavigationBundle&) =
      delete;
  PolicyContainerNavigationBundle& operator=(
      const PolicyContainerNavigationBundle&) = delete;
  PolicyContainerNavigationBundle(PolicyContainerNavigationBundle&&) = delete;
  PolicyContainerNavigationBundle& operator=(
      PolicyContainerNavigationBundle&&) = delete;

  // Returns a pointer to a snapshot of the parent's policies captured at
  // construction time. Returns nullptr if there was no parent.
  const PolicyContainerPolicies* ParentPolicies() const;

  // Returns a pointer to a snapshot of the navigation initiator's policies
  // captured at construction time. Returns nullptr if there was no initiator.
  const PolicyContainerPolicies* InitiatorPolicies() const;

  // Returns a pointer to a snapshot of the navigation history entry's policies
  // captured at construction time. Returns nullptr if there was no entry, of
  // if the entry had no policies.
  const PolicyContainerPolicies* HistoryPolicies() const;

  // Sets the cross origin opener policy of the new document.
  //
  // This must be called before |ComputePolicies()|.
  void SetCrossOriginOpenerPolicy(network::CrossOriginOpenerPolicy coop);

  // Sets the cross origin embedder policy of the new document.
  //
  // This must be called before |ComputePolicies()|.
  void SetCrossOriginEmbedderPolicy(network::CrossOriginEmbedderPolicy coep);

  // Sets the IP address space of the delivered policies of the new document.
  //
  // This must be called before |ComputePolicies()|.
  void SetIPAddressSpace(network::mojom::IPAddressSpace address_space);

  // Sets whether the origin of the document being navigated to is
  // potentially-trustworthy, as defined in:
  // https://w3c.github.io/webappsec-secure-contexts/#is-origin-trustworthy.
  //
  // This must be called before |ComputePolicies()|.
  void SetIsOriginPotentiallyTrustworthy(bool value);

  // Records an additional Content Security Policy that will apply to the new
  // document. |policy| must not be null. Policies added this way are ignored
  // for failed navigations and history navigations.
  void AddContentSecurityPolicy(
      network::mojom::ContentSecurityPolicyPtr policy);

  // Same as `AddContentSecurityPolicy` above, but takes a vector of policies.
  void AddContentSecurityPolicies(
      std::vector<network::mojom::ContentSecurityPolicyPtr> policies);

  // Returns the delivered policies, as set so far by:
  //
  //  - |SetIPAddressSpace()| for |ip_address_space|
  //  - |SetIsOriginPotentiallyTrustworthy()| and |ComputePolicies()| for
  //    |is_web_secure_context|
  const PolicyContainerPolicies& DeliveredPoliciesForTesting() const;

  // Sets final policies to defaults suitable for error pages, and builds a
  // policy container host.
  //
  // |is_inside_mhtml| Whether the navigation loads an MHTML document or a
  // subframe of an MHTML document. This is used by |ComputeSandboxFlags()|.
  // |frame_sandbox_flags| represents the frame's sandbox flags, these are used
  // by |ComputeSandboxFlags()|.
  //
  // This method must only be called once. However it can be called after
  // |ComputePolicies()|.
  void ComputePoliciesForError(
      bool is_inside_mhtml,
      network::mojom::WebSandboxFlags frame_sandbox_flags);

  // Sets final policies to their correct values and builds a policy container
  // host.
  //
  // |url| should designate the URL of the document after all redirects have
  // been followed.
  // |is_inside_mhtml| Whether the navigation loads an MHTML document or a
  // subframe of an MHTML document. This is used by |ComputeSandboxFlags()|.
  // |frame_sandbox_flags| The frame's sandbox flags, these are used
  // by |ComputeSandboxFlags()|.
  //
  // Also sets |DeliveredPolicies().is_web_secure_context| to its final value.
  //
  // This method must only be called once. |ComputePoliciesForError()| may be
  // called later and this override the final policies.
  void ComputePolicies(const GURL& url,
                       bool is_inside_mhtml,
                       network::mojom::WebSandboxFlags frame_sandbox_flags);

  // Returns a reference to the policies of the new document, i.e. the policies
  // in the policy container host to be committed.
  //
  // |ComputePolicies()| or |ComputePoliciesForError()| must have been called
  // previously.
  const PolicyContainerPolicies& FinalPolicies() const;

  // Creates a PolicyContainer connected to this bundle's PolicyContainerHost.
  //
  // Should only be called once. |ComputePolicies()| or
  // |ComputePoliciesForError()| must have been called previously.
  blink::mojom::PolicyContainerPtr CreatePolicyContainerForBlink();

  // Moves the PolicyContainerHost out of this bundle. The returned host
  // contains the same policies as |FinalPolicies()|.
  //
  // |ComputePolicies()| or |ComputePoliciesForError()| must have been called
  // previously.
  scoped_refptr<PolicyContainerHost> TakePolicyContainerHost() &&;

  // Called by same-document navigation requests that need to be restarted as
  // cross-document navigations. This happens when a same-document commit fails
  // due to another navigation committing in the meantime. This resets the
  // PolicyContainerNavigationBundle to the state when it was first created.
  void ResetForCrossDocumentRestart();

 private:
  // Whether either of |ComputePolicies()| or |ComputePoliciesForError()| has
  // been called yet.
  bool HasComputedPolicies() const;

  // Sets |delivered_policies_.is_web_secure_context| to its final value.
  //
  // Helper for |ComputePolicies()|.
  void ComputeIsWebSecureContext();

  // Sets |policies.sandbox_flags| to its final value. This merges the CSP
  // sandbox flags with the frame's sandbox flag.
  //
  // |is_inside_mhtml| Whether the navigation loads an MHTML document or a
  // subframe of an MHTML document. When true, this forces all sandbox flags on
  // the document except popups and popups-to-escape-sandbox.
  // |frame_sandbox_flags| The frame's sandbox flags.
  // |policies| The policies computed for the document except for the sandbox
  // flags.
  //
  // Helper for |ComputePolicies()| and |ComputePoliciesForError()|.
  void ComputeSandboxFlags(bool is_inside_mhtml,
                           network::mojom::WebSandboxFlags frame_sandbox_flags,
                           PolicyContainerPolicies* policies);

  // Sets |host_|.
  void SetFinalPolicies(std::unique_ptr<PolicyContainerPolicies> policies);

  // Helper for `FinalizePolicies()`. Appends the delivered Content Security
  // Policies to `policies` and returns them.
  std::unique_ptr<PolicyContainerPolicies> IncorporateDeliveredPolicies(
      const GURL& url,
      std::unique_ptr<PolicyContainerPolicies> policies);

  // Helper for `FinalizePolicies()`. Returns, depending on `url`, the policies
  // that this document inherits from parent/initiator.
  std::unique_ptr<PolicyContainerPolicies> ComputeInheritedPolicies(
      const GURL& url);

  // Helper for `FinalizePolicies()`. Returns, depending on `url`, the final
  // policies for the document that is going to be committed.
  std::unique_ptr<PolicyContainerPolicies> ComputeFinalPolicies(
      const GURL& url,
      bool is_inside_mhtml,
      network::mojom::WebSandboxFlags frame_sandbox_flags);

  // The policies of the parent document, if any.
  const std::unique_ptr<PolicyContainerPolicies> parent_policies_;

  // The policies of the document that initiated the navigation, if any.
  const std::unique_ptr<PolicyContainerPolicies> initiator_policies_;

  // The policies restored from the history navigation entry, if any.
  const std::unique_ptr<PolicyContainerPolicies> history_policies_;

  // The policies extracted from the response as it is loaded.
  //
  // See the comment on |SetIsOriginPotentiallyTrustworthy()| regarding this
  // member's |is_web_secure_context| field.
  std::unique_ptr<PolicyContainerPolicies> delivered_policies_;

  // Nullptr until |ComputePolicies()| or |ComputePoliciesForError()| is
  // called, then moved from by |TakePolicyContainerHost()|.
  scoped_refptr<PolicyContainerHost> host_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_POLICY_CONTAINER_NAVIGATION_BUNDLE_H_
