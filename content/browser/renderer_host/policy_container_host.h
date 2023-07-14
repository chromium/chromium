// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_POLICY_CONTAINER_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_POLICY_CONTAINER_HOST_H_

#include <iosfwd>

#include "content/browser/child_process_host_impl.h"
#include "content/common/content_export.h"
#include "content/public/browser/content_browser_client.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "services/network/public/cpp/cross_origin_embedder_policy.h"
#include "services/network/public/cpp/cross_origin_opener_policy.h"
#include "services/network/public/cpp/web_sandbox_flags.h"
#include "services/network/public/mojom/content_security_policy.mojom-forward.h"
#include "services/network/public/mojom/ip_address_space.mojom-shared.h"
#include "services/network/public/mojom/referrer_policy.mojom-shared.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/frame/policy_container.mojom.h"
#include "url/gurl.h"

namespace content {

// The contents of a PolicyContainerHost.
struct CONTENT_EXPORT PolicyContainerPolicies {
  PolicyContainerPolicies();

  PolicyContainerPolicies(
      network::mojom::ReferrerPolicy referrer_policy,
      network::mojom::IPAddressSpace ip_address_space,
      bool is_web_secure_context,
      std::vector<network::mojom::ContentSecurityPolicyPtr>
          content_security_policies,
      const network::CrossOriginOpenerPolicy& cross_origin_opener_policy,
      const network::CrossOriginEmbedderPolicy& cross_origin_embedder_policy,
      network::mojom::WebSandboxFlags sandbox_flags,
      bool is_credentialless,
      bool can_navigate_top_without_user_gesture,
      bool allow_cross_origin_isolation);

  explicit PolicyContainerPolicies(
      const blink::mojom::PolicyContainerPolicies& policies);

  // Used when loading workers from network schemes.
  // WARNING: This does not populate referrer policy.
  PolicyContainerPolicies(const GURL& url,
                          network::mojom::URLResponseHead* response_head,
                          ContentBrowserClient* client);

  // Instances of this type are move-only.
  PolicyContainerPolicies(const PolicyContainerPolicies&) = delete;
  PolicyContainerPolicies& operator=(const PolicyContainerPolicies&) = delete;
  PolicyContainerPolicies(PolicyContainerPolicies&&);
  PolicyContainerPolicies& operator=(PolicyContainerPolicies&&);

  ~PolicyContainerPolicies();

  // Returns an identical copy of this instance.
  PolicyContainerPolicies Clone() const;

  // Returns the result of `Clone()` stored on the heap.
  std::unique_ptr<PolicyContainerPolicies> ClonePtr() const;

  // Helper function to append items to `content_security_policies`.
  void AddContentSecurityPolicies(
      std::vector<network::mojom::ContentSecurityPolicyPtr> policies);

  blink::mojom::PolicyContainerPoliciesPtr ToMojoPolicyContainerPolicies()
      const;

  // The referrer policy for the associated document. If not overwritten via a
  // call to SetReferrerPolicy (for example after parsing the Referrer-Policy
  // header or a meta tag), the default referrer policy will be applied to the
  // document.
  network::mojom::ReferrerPolicy referrer_policy =
      network::mojom::ReferrerPolicy::kDefault;

  // The IPAddressSpace associated with the document. In all non-network pages
  // (srcdoc, data urls, etc.) where we don't have an IP address to work with,
  // it is inherited following the general rules of the PolicyContainerHost.
  network::mojom::IPAddressSpace ip_address_space =
      network::mojom::IPAddressSpace::kUnknown;

  // Whether the document is a secure context.
  //
  // See: https://html.spec.whatwg.org/C/#secure-contexts.
  //
  // See also:
  //  - |network::IsUrlPotentiallyTrustworthy()|
  //  - |network::IsOriginPotentiallyTrustworthy()|
  bool is_web_secure_context = false;

  // The content security policies of the associated document.
  std::vector<network::mojom::ContentSecurityPolicyPtr>
      content_security_policies;

  // The cross-origin-opener-policy (COOP) of the document
  // See:
  // https://html.spec.whatwg.org/multipage/origin.html#cross-origin-opener-policies
  network::CrossOriginOpenerPolicy cross_origin_opener_policy;

  // The cross-origin-embedder-policy (COEP) of the document
  // See:
  // https://html.spec.whatwg.org/multipage/origin.html#coep
  network::CrossOriginEmbedderPolicy cross_origin_embedder_policy;

  // Tracks the sandbox flags which are in effect on this document. This
  // includes any flags which have been set by a Content-Security-Policy header,
  // in addition to those which are set by the embedding frame.
  network::mojom::WebSandboxFlags sandbox_flags =
      network::mojom::WebSandboxFlags::kNone;

  // https://wicg.github.io/anonymous-iframe/#spec-window-attribute
  // True for window framed inside credentialless iframe, directly or indirectly
  // by one of its ancestors
  bool is_credentialless = false;

  // Tracks if a document is allowed to navigate the top-level frame without
  // sticky user activation. A document loses this ability when it is
  // cross-origin with the top-level frame. An exception is made if the parent
  // embeds the child with sandbox="allow-top-navigation", as opposed to not
  // using sandboxing. A document that is same-origin to the top-level frame
  // will always have this value set to true.
  bool can_navigate_top_without_user_gesture = true;

  // The top-level initial empty document opened as a popup by a cross-origin
  // iframe might inherit the COOP policies of the top-level document but it
  // shouldn't have crossOriginIsolated capabilities if COOP was initially set
  // by another origin. Hence, we pass down this boolean to tell the renderer to
  // restrict those capabilities. For more detail, see
  // https://github.com/hemeryar/coi-with-popups/blob/main/docs/cross_origin_iframe_popup.MD
  bool allow_cross_origin_isolation = false;
};

// PolicyContainerPolicies structs are comparable for equality.
CONTENT_EXPORT bool operator==(const PolicyContainerPolicies& lhs,
                               const PolicyContainerPolicies& rhs);
CONTENT_EXPORT bool operator!=(const PolicyContainerPolicies& lhs,
                               const PolicyContainerPolicies& rhs);

// Streams a human-readable string representation of |policies| to |out|.
CONTENT_EXPORT std::ostream& operator<<(
    std::ostream& out,
    const PolicyContainerPolicies& policies);

// PolicyContainerHost serves as a container for several security policies. It
// should be owned by a RenderFrameHost. It keep tracks of the policies assigned
// to a document. When a document creates/opens another document with a local
// scheme (about:blank, about:srcdoc, data, blob, filesystem), the
// PolicyContainerHost of the opener is cloned and a copy is attached to the new
// document, so that the same security policies are applied to it. It implements
// a mojo interface that allows updates coming from Blink.
//
// Although it is owned through a scoped_refptr, a PolicyContainerHost should
// not be shared between different owners. A RenderFrameHost gets a
// PolicyContainerHost at creation time, and it gets a new one from the
// NavigationRequest every time a NavigationRequest commits. Initially, a
// PolicyContainerHost has no associated frame token. As soon as the
// PolicyContainerHost becomes owned by a RenderFrameHost, the method
// AssociateWithFrameToken must be called. This makes it possible to retrieve
// the PolicyContainerHost via
// PolicyContainerHost::FromFrameToken. Additionally, this enables the
// PolicyContainerHost to outlive its RenderFrameHost. In fact, as long as the
// mojo receiver or a keep alive handle (as registered using
// IssueKeepAliveHandle) is alive, the PolicyContainerHost will still be
// retrievable by the corresponding frame token even if the RenderFrameHost has
// been deleted (and the scoped_refptr with it).
class CONTENT_EXPORT PolicyContainerHost
    : public base::RefCounted<PolicyContainerHost>,
      public blink::mojom::PolicyContainerHost {
 public:
  // Constructs a PolicyContainerHost containing default policies and an unbound
  // mojo receiver.
  PolicyContainerHost();

  // Constructs a PolicyContainerHost containing the given |policies|.
  explicit PolicyContainerHost(PolicyContainerPolicies policies);

  // PolicyContainerHost instances are neither copyable nor movable.
  PolicyContainerHost(const PolicyContainerHost&) = delete;
  PolicyContainerHost& operator=(const PolicyContainerHost&) = delete;

  // Retrieve the PolicyContainerHost associated with the frame token |token|
  // (cf. AsssociateWithFrameToken).
  static PolicyContainerHost* FromFrameToken(
      const blink::LocalFrameToken& token);

  // AssociateWithFrameToken must be called as soon as this PolicyContainerHost
  // becomes owned by a RenderFrameHost. After this function is called, it
  // becomes possible to retrieve this PolicyContainerHost via
  // PolicyContainerHost::FromFrameToken. This function can be called only once.
  void AssociateWithFrameToken(
      const blink::LocalFrameToken& token,
      int process_id = ChildProcessHost::kInvalidUniqueID);

  const PolicyContainerPolicies& policies() const { return policies_; }

  network::mojom::ReferrerPolicy referrer_policy() const {
    return policies_.referrer_policy;
  }

  network::mojom::IPAddressSpace ip_address_space() const {
    return policies_.ip_address_space;
  }

  network::CrossOriginOpenerPolicy& cross_origin_opener_policy() {
    return policies_.cross_origin_opener_policy;
  }

  const network::CrossOriginEmbedderPolicy& cross_origin_embedder_policy()
      const {
    return policies_.cross_origin_embedder_policy;
  }

  network::mojom::WebSandboxFlags sandbox_flags() const {
    return policies_.sandbox_flags;
  }

  void AddContentSecurityPolicies(
      std::vector<network::mojom::ContentSecurityPolicyPtr>
          content_security_policies) final;

  void set_cross_origin_opener_policy(
      const network::CrossOriginOpenerPolicy& policy) {
    policies_.cross_origin_opener_policy = policy;
  }

  void set_cross_origin_embedder_policy(
      const network::CrossOriginEmbedderPolicy& policy) {
    policies_.cross_origin_embedder_policy = policy;
  }

  // Merges the provided sandbox flags with the existing flags.
  void set_sandbox_flags(network::mojom::WebSandboxFlags sandbox_flags) {
    policies_.sandbox_flags = sandbox_flags;
  }

  void SetIsCredentialless() { policies_.is_credentialless = true; }

  void SetCanNavigateTopWithoutUserGesture(bool value) {
    policies_.can_navigate_top_without_user_gesture = value;
  }

  void SetAllowCrossOriginIsolation(bool value) {
    policies_.allow_cross_origin_isolation = value;
  }

  // Return a PolicyContainer containing copies of the policies and a pending
  // mojo remote that can be used to update policies in this object. If called a
  // second time, it resets the receiver and creates a new PolicyContainer,
  // invalidating the remote of the previous one.
  blink::mojom::PolicyContainerPtr CreatePolicyContainerForBlink();

  // Create a new PolicyContainerHost with the same policies (i.e. a deep copy),
  // but with a new, unbound mojo receiver.
  scoped_refptr<PolicyContainerHost> Clone() const;

  // Bind this PolicyContainerHost with the given mojo receiver, so that it can
  // handle mojo messages coming from the corresponding remote.
  void Bind(
      blink::mojom::PolicyContainerBindParamsPtr policy_container_bind_params);

  // Register a keep alive handle by passing the mojo receiver. The
  // PolicyContainerHost is kept alive as long as the corresponding remote
  // exists.
  // See also:
  // - PolicyContainerHost::AssociateWithFrameToken(token)
  // - PolicyContainerHost::FromFrameToken(token)
  void IssueKeepAliveHandle(
      mojo::PendingReceiver<blink::mojom::PolicyContainerHostKeepAliveHandle>
          receiver) override;

 private:
  friend class base::RefCounted<PolicyContainerHost>;
  ~PolicyContainerHost() override;

  void SetReferrerPolicy(network::mojom::ReferrerPolicy referrer_policy) final;

  // The policies of this PolicyContainerHost.
  PolicyContainerPolicies policies_;

  mojo::AssociatedReceiver<blink::mojom::PolicyContainerHost>
      policy_container_host_receiver_{this};

  mojo::UniqueReceiverSet<blink::mojom::PolicyContainerHostKeepAliveHandle>
      keep_alive_handles_receiver_set_;

  absl::optional<blink::LocalFrameToken> frame_token_ = absl::nullopt;
  int process_id_ = ChildProcessHost::kInvalidUniqueID;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_POLICY_CONTAINER_HOST_H_
