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

  // Returns the delivered policies as informed by |SetIPAddressSpace()|.
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

  // Returns the policies to use instead of |delivered_policies_|, if any.
  //
  // For example, if |url| is `about:srcdoc`, returns |&*parent_policies_|.
  //
  // Helper for |FinalizePolicies()|.
  const PolicyContainerPolicies* ComputeFinalPoliciesOverride(
      const GURL& url) const;

  // Sets |host_|.
  void SetFinalPolicies(const PolicyContainerPolicies& policies);

  // The policies of the parent document, if any.
  const std::unique_ptr<const PolicyContainerPolicies> parent_policies_;

  // The policies of the document that initiated the navigation, if any.
  const std::unique_ptr<const PolicyContainerPolicies> initiator_policies_;

  // The policies restored from the history navigation entry, if any.
  const std::unique_ptr<const PolicyContainerPolicies> history_policies_;

  // The policies extracted from the response as it is loaded.
  PolicyContainerPolicies delivered_policies_;

  // Nullptr until |FinalizePolicies()| or |FinalizePoliciesForError()| is
  // called, then moved from by |TakePolicyContainerHost()|.
  scoped_refptr<PolicyContainerHost> host_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_POLICY_CONTAINER_NAVIGATION_BUNDLE_H_
