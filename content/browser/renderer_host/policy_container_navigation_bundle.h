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
// At ready-to-commit time, |FinalizePolicies()| or |FinalizePoliciesForError()|
// can be called to set the final policies of the new document and create a new
// policy container host.
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

  // Sets the IP address space of the delivered policies of the new document.
  //
  // This instance must not be finalized.
  void SetIPAddressSpace(network::mojom::IPAddressSpace address_space);

  // Sets whether the origin of the document being navigated to is
  // potentially-trustworthy, as defined in:
  // https://w3c.github.io/webappsec-secure-contexts/#is-origin-trustworthy.
  //
  // This instance must not be frozen.
  void SetIsOriginPotentiallyTrustworthy(bool value);

  // Returns the delivered policies, as set so far by:
  //
  //  - |SetIPAddressSpace()| for |ip_address_space|
  //  - |SetIsOriginPotentiallyTrustworthy()| and |FinalizePolicies()| for
  //    |is_web_secure_context|
  //
  // TODO(titouan): Consider exposing individual accessors instead, since the
  // semantics of the policy fields do not correspond to those documented on
  // the struct itself - we just happen to store data with a similar shape.
  const PolicyContainerPolicies& DeliveredPolicies() const;

  // Sets final policies to defaults suitable for error pages, and builds a
  // policy container host.
  //
  // This method must only be called once, and is mutually exclusive with
  // |FinalizePolicies()|.
  void FinalizePoliciesForError();

  // Sets final policies to their correct values and builds a policy container
  // host.
  //
  // |url| should designate the URL of the document after all redirects have
  // been followed.
  //
  // Also sets |DeliveredPolicies().is_web_secure_context| to its final value.
  //
  // This method must only be called once, and is mutually exclusive with
  // |FinalizePoliciesForError()|.
  void FinalizePolicies(const GURL& url);

  // Returns a reference to the policies of the new document, i.e. the policies
  // in the policy container host to be committed.
  //
  // This instance must be finalized.
  const PolicyContainerPolicies& FinalPolicies() const;

  // Creates a PolicyContainer connected to this bundle's PolicyContainerHost.
  //
  // Should only be called once. This instance must be finalized.
  blink::mojom::PolicyContainerPtr CreatePolicyContainerForBlink();

  // Moves the PolicyContainerHost out of this bundle. The returned host
  // contains the same policies as |FinalPolicies()|.
  //
  // This instance must be finalized.
  scoped_refptr<PolicyContainerHost> TakePolicyContainerHost() &&;

 private:
  // Whether either of |FinalizePolicies()| or |FinalizePoliciesForError()| has
  // been called yet.
  bool IsFinalized() const;

  // Sets |delivered_policies_.is_web_secure_context| to its final value.
  //
  // Helper for |FreezeFinalPolicies()|.
  void FinalizeIsWebSecureContext();

  // Sets |host_|.
  void SetFinalPolicies(std::unique_ptr<PolicyContainerPolicies> policies);

  // The policies of the parent document, if any.
  std::unique_ptr<PolicyContainerPolicies> parent_policies_;

  // The policies of the document that initiated the navigation, if any.
  std::unique_ptr<PolicyContainerPolicies> initiator_policies_;

  // The policies restored from the history navigation entry, if any.
  std::unique_ptr<PolicyContainerPolicies> history_policies_;

  // The policies extracted from the response as it is loaded.
  //
  // See the comment on |SetIsOriginPotentiallyTrustworthy()| regarding this
  // member's |is_web_secure_context| field.
  std::unique_ptr<PolicyContainerPolicies> delivered_policies_;

  // Nullptr until |FinalizePolicies()| or |FinalizePoliciesForError()| is
  // called, then moved from by |TakePolicyContainerHost()|.
  scoped_refptr<PolicyContainerHost> host_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_POLICY_CONTAINER_NAVIGATION_BUNDLE_H_
