// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/policy_container_host.h"

#include <algorithm>

#include "base/memory/scoped_refptr.h"
#include "content/browser/renderer_host/frame_navigation_entry.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/local_network_access_util.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_thread.h"
#include "services/network/public/cpp/cross_origin_opener_policy.h"
#include "services/network/public/cpp/document_isolation_policy.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "services/network/public/mojom/integrity_policy.mojom.h"

namespace {
template <typename T>
std::string ConvertToString(const std::vector<T>& array) {
  std::ostringstream oss;
  size_t array_size = array.size();
  for (size_t i = 0; i < array_size; ++i) {
    oss << array[i];
    if (i == array_size - 1) {
      oss << ", ";
    }
  }
  return oss.str();
}
}  // namespace

namespace content {

std::ostream& operator<<(std::ostream& out,
                         const PolicyContainerPolicies& policies) {
  out << "{ referrer_policy: " << policies.referrer_policy
      << ", ip_address_space: " << policies.ip_address_space
      << ", is_web_secure_context: " << policies.is_web_secure_context;

  out << ", connection_allowlists: " << "{";
  if (policies.connection_allowlists.enforced.has_value()) {
    out << " enforced: { allowlist: [";
    for (size_t i = 0;
         i < policies.connection_allowlists.enforced->allowlist.size(); i++) {
      out << policies.connection_allowlists.enforced->allowlist[i];
      if (i < policies.connection_allowlists.enforced->allowlist.size() - 1) {
        out << ", ";
      }
    }
    out << "] }";
  }

  out << "}, content_security_policies: ";
  if (policies.content_security_policies.empty()) {
    out << "[]";
  } else {
    out << "[ ";
    auto it = policies.content_security_policies.begin();
    for (; it + 1 != policies.content_security_policies.end(); ++it) {
      out << (*it)->header->header_value << ", ";
    }
    out << (*it)->header->header_value << " ]";
  }

  out << ", cross_origin_opener_policy: "
      << "{ value: " << policies.cross_origin_opener_policy.value
      << ", reporting_endpoint: "
      << policies.cross_origin_opener_policy.reporting_endpoint.value_or(
             "<null>")
      << ", report_only_value: "
      << policies.cross_origin_opener_policy.report_only_value
      << ", report_only_reporting_endpoint: "
      << policies.cross_origin_opener_policy.report_only_reporting_endpoint
             .value_or("<null>")
      << ", soap_by_default_value: "
      << policies.cross_origin_opener_policy.soap_by_default_value << " }";

  out << ", cross_origin_embedder_policy: "
      << "{ value: " << policies.cross_origin_embedder_policy.value
      << ", reporting_endpoint: "
      << policies.cross_origin_embedder_policy.reporting_endpoint.value_or(
             "<null>")
      << ", report_only_value: "
      << policies.cross_origin_embedder_policy.report_only_value
      << ", report_only_reporting_endpoint: "
      << policies.cross_origin_embedder_policy.report_only_reporting_endpoint
             .value_or("<null>")
      << " }";

  out << ", document_isolation_policy: " << "{ value: "
      << policies.document_isolation_policy.value << ", reporting_endpoint: "
      << policies.document_isolation_policy.reporting_endpoint.value_or(
             "<null>")
      << ", report_only_value: "
      << policies.document_isolation_policy.report_only_value
      << ", report_only_reporting_endpoint: "
      << policies.document_isolation_policy.report_only_reporting_endpoint
             .value_or("<null>")
      << " }";

  out << ", integrity_policy: " << "{ blocked-destinations: "
      << ConvertToString<::network::mojom::IntegrityPolicy_Destination>(
             policies.integrity_policy.blocked_destinations)
      << ", sources: "
      << ConvertToString<::network::mojom::IntegrityPolicy_Source>(
             policies.integrity_policy.sources)
      << ", endpoints: "
      << ConvertToString<std::string>(policies.integrity_policy.endpoints)
      << " }";

  out << ", sandbox_flags: " << policies.sandbox_flags;
  out << ", is_credentialless: " << policies.is_credentialless;
  out << ", can_navigate_top_without_user_gesture: "
      << policies.can_navigate_top_without_user_gesture;
  out << ", cross_origin_isolationi_enabled_by_dip: "
      << policies.cross_origin_isolation_enabled_by_dip;

  return out << " }";
}

PolicyContainerPolicies::PolicyContainerPolicies() = default;

PolicyContainerPolicies::PolicyContainerPolicies(
    network::mojom::ReferrerPolicy referrer_policy,
    network::mojom::IPAddressSpace ip_address_space,
    bool is_web_secure_context,
    network::ConnectionAllowlists connection_allowlists,
    std::vector<network::mojom::ContentSecurityPolicyPtr>
        content_security_policies,
    const network::CrossOriginOpenerPolicy& cross_origin_opener_policy,
    const network::CrossOriginEmbedderPolicy& cross_origin_embedder_policy,
    const network::DocumentIsolationPolicy& document_isolation_policy,
    network::IntegrityPolicy integrity_policy,
    network::IntegrityPolicy integrity_policy_report_only,
    network::mojom::WebSandboxFlags sandbox_flags,
    bool is_credentialless,
    bool can_navigate_top_without_user_gesture,
    bool cross_origin_isolation_enabled_by_dip,
    const std::optional<AgentClusterKey::CrossOriginIsolationKey>& coi_key)
    : referrer_policy(referrer_policy),
      ip_address_space(ip_address_space),
      is_web_secure_context(is_web_secure_context),
      connection_allowlists(std::move(connection_allowlists)),
      content_security_policies(std::move(content_security_policies)),
      cross_origin_opener_policy(cross_origin_opener_policy),
      cross_origin_embedder_policy(cross_origin_embedder_policy),
      document_isolation_policy(document_isolation_policy),
      cross_origin_isolation_key_override(coi_key),
      integrity_policy(std::move(integrity_policy)),
      integrity_policy_report_only(std::move(integrity_policy_report_only)),
      sandbox_flags(sandbox_flags),
      is_credentialless(is_credentialless),
      can_navigate_top_without_user_gesture(
          can_navigate_top_without_user_gesture),
      cross_origin_isolation_enabled_by_dip(
          cross_origin_isolation_enabled_by_dip) {}

PolicyContainerPolicies::PolicyContainerPolicies(
    const blink::mojom::PolicyContainerPolicies& policies,
    bool is_web_secure_context)
    : PolicyContainerPolicies(policies.referrer_policy,
                              policies.ip_address_space,
                              is_web_secure_context,
                              policies.connection_allowlists,
                              mojo::Clone(policies.content_security_policies),
                              network::CrossOriginOpenerPolicy(),
                              policies.cross_origin_embedder_policy,
                              network::DocumentIsolationPolicy(),
                              policies.integrity_policy,
                              policies.integrity_policy_report_only,
                              policies.sandbox_flags,
                              policies.is_credentialless,
                              policies.can_navigate_top_without_user_gesture,
                              policies.cross_origin_isolation_enabled_by_dip,
                              /*coi_key=*/std::nullopt) {}

PolicyContainerPolicies::PolicyContainerPolicies(
    const GURL& url,
    network::mojom::URLResponseHead* response_head,
    ContentBrowserClient* client)
    : PolicyContainerPolicies(
          network::mojom::ReferrerPolicy::kDefault,
          CalculateIPAddressSpace(url, response_head, client),
          network::IsUrlPotentiallyTrustworthy(url),
          response_head->parsed_headers->connection_allowlists,
          mojo::Clone(response_head->parsed_headers->content_security_policy),
          response_head->parsed_headers->cross_origin_opener_policy,
          response_head->parsed_headers->cross_origin_embedder_policy,
          response_head->parsed_headers->document_isolation_policy,
          response_head->parsed_headers->integrity_policy,
          response_head->parsed_headers->integrity_policy_report_only,
          network::mojom::WebSandboxFlags::kNone,
          /*is_credentialless=*/false,
          /*can_navigate_top_without_user_gesture=*/true,
          /*cross_origin_isolation_enabled_by_dip=*/false,
          /*coi_key=*/std::nullopt) {
  for (auto& content_security_policy :
       response_head->parsed_headers->content_security_policy) {
    sandbox_flags |= content_security_policy->sandbox;
  }
}

PolicyContainerPolicies::PolicyContainerPolicies(PolicyContainerPolicies&&) =
    default;

PolicyContainerPolicies& PolicyContainerPolicies::operator=(
    PolicyContainerPolicies&&) = default;

PolicyContainerPolicies::~PolicyContainerPolicies() = default;

PolicyContainerPolicies PolicyContainerPolicies::Clone() const {
  return PolicyContainerPolicies(
      referrer_policy, ip_address_space, is_web_secure_context,
      connection_allowlists, mojo::Clone(content_security_policies),
      cross_origin_opener_policy, cross_origin_embedder_policy,
      mojo::Clone(document_isolation_policy), integrity_policy,
      integrity_policy_report_only, sandbox_flags, is_credentialless,
      can_navigate_top_without_user_gesture,
      cross_origin_isolation_enabled_by_dip,
      cross_origin_isolation_key_override);
}

std::unique_ptr<PolicyContainerPolicies> PolicyContainerPolicies::ClonePtr()
    const {
  return std::make_unique<PolicyContainerPolicies>(Clone());
}

void PolicyContainerPolicies::AddContentSecurityPolicies(
    std::vector<network::mojom::ContentSecurityPolicyPtr> policies) {
  content_security_policies.insert(content_security_policies.end(),
                                   std::make_move_iterator(policies.begin()),
                                   std::make_move_iterator(policies.end()));
}

blink::mojom::PolicyContainerPoliciesPtr
PolicyContainerPolicies::ToMojoPolicyContainerPolicies() const {
  return blink::mojom::PolicyContainerPolicies::New(
      connection_allowlists, cross_origin_embedder_policy, integrity_policy,
      integrity_policy_report_only, referrer_policy,
      mojo::Clone(content_security_policies), is_credentialless, sandbox_flags,
      ip_address_space, can_navigate_top_without_user_gesture,
      cross_origin_isolation_enabled_by_dip);
}

PolicyContainerHost::PolicyContainerHost() = default;

PolicyContainerHost::PolicyContainerHost(PolicyContainerPolicies policies)
    : policies_(std::move(policies)) {}

PolicyContainerHost::~PolicyContainerHost() = default;

void PolicyContainerHost::AddContentSecurityPoliciesForTesting(
    std::vector<network::mojom::ContentSecurityPolicyPtr>
        content_security_policies) {
  AddContentSecurityPolicies(std::move(content_security_policies),
                             base::UnguessableToken::Create());
}

void PolicyContainerHost::SetReferrerPolicyForTesting(
    network::mojom::ReferrerPolicy referrer_policy) {
  policies_.referrer_policy = referrer_policy;
  if (client_) {
    client_->DidChangeReferrerPolicy(referrer_policy);
  }
}

void PolicyContainerHost::SetReferrerPolicy(
    network::mojom::ReferrerPolicy referrer_policy,
    const base::UnguessableToken& new_initiator_state_token) {
  policies_.referrer_policy = referrer_policy;
  if (client_) {
    client_->DidChangeReferrerPolicy(referrer_policy);
    client_->DidUpdateInitiatorStateToken(new_initiator_state_token);
  }
}

void PolicyContainerHost::AddContentSecurityPolicies(
    std::vector<network::mojom::ContentSecurityPolicyPtr>
        content_security_policies,
    const base::UnguessableToken& new_initiator_state_token) {
  policies_.AddContentSecurityPolicies(std::move(content_security_policies));
  if (client_) {
    client_->DidUpdateInitiatorStateToken(new_initiator_state_token);
  }
}

blink::mojom::PolicyContainerPtr
PolicyContainerHost::CreatePolicyContainerForBlink() {
  // This function might be called several times, for example if we need to
  // recreate the RenderFrame after the renderer process died. We gracefully
  // handle this by resetting the receiver and creating a new one. It would be
  // good to find a way to check that the previous remote has been deleted or is
  // not needed anymore. Unfortunately, this cannot be done with a disconnect
  // handler, since the mojo disconnect notification is not guaranteed to be
  // received before we try to create a new remote.
  policy_container_host_receiver_.reset();
  mojo::PendingAssociatedRemote<blink::mojom::PolicyContainerHost> remote;
  Bind(blink::mojom::PolicyContainerBindParams::New(
      remote.InitWithNewEndpointAndPassReceiver()));

  return blink::mojom::PolicyContainer::New(
      policies_.ToMojoPolicyContainerPolicies(), std::move(remote));
}

scoped_refptr<PolicyContainerHost> PolicyContainerHost::Clone() const {
  return base::MakeRefCounted<PolicyContainerHost>(policies_.Clone());
}

void PolicyContainerHost::Bind(
    blink::mojom::PolicyContainerBindParamsPtr bind_params) {
  policy_container_host_receiver_.Bind(std::move(bind_params->receiver));

  // Keep the PolicyContainerHost alive, as long as its PolicyContainer (owning
  // the mojo remote) in the renderer process alive.
  scoped_refptr<PolicyContainerHost> copy = this;
  policy_container_host_receiver_.set_disconnect_handler(base::BindOnce(
      [](scoped_refptr<PolicyContainerHost>) {}, std::move(copy)));
}

void PolicyContainerHost::SetClient(Client* client) {
  CHECK_CURRENTLY_ON(BrowserThread::UI);
  client_ = client;
}

}  // namespace content
