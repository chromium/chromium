/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>

#include "base/containers/contains.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/blob/blob_url.h"
#include "third_party/blink/renderer/platform/blob/blob_url_null_origin_map.h"
#include "third_party/blink/renderer/platform/weborigin/known_ports.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/origin_access_entry.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"
#include "url/scheme_host_port.h"
#include "url/url_canon.h"
#include "url/url_canon_ip.h"
#include "url/url_constants.h"
#include "url/url_util.h"

namespace blink {

namespace {

const String& EnsureNonNull(const String& string) {
  if (string.IsNull())
    return g_empty_string;
  return string;
}

}  // namespace

bool SecurityOrigin::ShouldUseInnerURL(const KURL& url) {
  // FIXME: Blob URLs don't have inner URLs. Their form is
  // "blob:<inner-origin>/<UUID>", so treating the part after "blob:" as a URL
  // is incorrect.
  if (url.ProtocolIs("blob"))
    return true;
  if (url.ProtocolIs("filesystem"))
    return true;
  return false;
}

// In general, extracting the inner URL varies by scheme. It just so happens
// that all the URL schemes we currently support that use inner URLs for their
// security origin can be parsed using this algorithm.
KURL SecurityOrigin::ExtractInnerURL(const KURL& url) {
  if (url.InnerURL())
    return *url.InnerURL();
  // FIXME: Update this callsite to use the innerURL member function when
  // we finish implementing it.
  return KURL(url.GetPath().ToString());
}

// Note: When changing ShouldTreatAsOpaqueOrigin, consider also updating
// IsValidInput in //url/scheme_host_port.cc (there might be existing
// differences in behavior between these 2 layers, but we should avoid
// introducing new differences).
static bool ShouldTreatAsOpaqueOrigin(const KURL& url) {
  if (!url.IsValid())
    return true;

  KURL relevant_url;
  if (SecurityOrigin::ShouldUseInnerURL(url)) {
    relevant_url = SecurityOrigin::ExtractInnerURL(url);
    if (!relevant_url.IsValid())
      return true;
    // If the inner URL is also wrapped, the URL is invalid, so treat as opqaue.
    if (SecurityOrigin::ShouldUseInnerURL(relevant_url))
      return true;
  } else {
    relevant_url = url;
  }

  // URLs with schemes that require an authority, but which don't have one,
  // will have failed the isValid() test; e.g. valid HTTP URLs must have a
  // host.
  DCHECK(!((relevant_url.ProtocolIsInHTTPFamily() ||
            relevant_url.ProtocolIs("ftp")) &&
           relevant_url.Host().empty()));

  if (base::Contains(url::GetNoAccessSchemes(),
                     relevant_url.Protocol().Ascii()))
    return true;

  // Nonstandard schemes and unregistered schemes are placed in opaque origins.
  if (!relevant_url.IsStandard()) {
    // A temporary exception is made for non-standard local schemes.
    // TODO: Migrate "content:" and "externalfile:" to be standard schemes, and
    // remove the local scheme exception.
    if (base::Contains(url::GetLocalSchemes(), relevant_url.Protocol().Ascii()))
      return false;

    // Otherwise, treat non-standard origins as opaque, unless the Android
    // WebView workaround is enabled. If the workaround is enabled, return false
    // so that the scheme is retained, to avoid breaking XHRs on custom schemes,
    // et cetera.
    return !url::AllowNonStandardSchemesForAndroidWebView();
  }

  // This is the common case.
  return false;
}

scoped_refptr<SecurityOrigin> SecurityOrigin::CreateInternal(const KURL& url) {
  if (url::SchemeHostPort::ShouldDiscardHostAndPort(url.Protocol().Ascii())) {
    return base::AdoptRef(
        new SecurityOrigin(url.Protocol(), g_empty_string, 0));
  }

  // This mimics the logic in url::SchemeHostPort(const GURL&). In
  // particular, it ensures a URL with a port of 0 will translate into
  // an origin with an effective port of 0.
  uint16_t port = (url.HasPort() || !url.IsValid() || !url.IsStandard())
                      ? url.Port()
                      : DefaultPortForProtocol(url.Protocol());
  return base::AdoptRef(new SecurityOrigin(EnsureNonNull(url.Protocol()),
                                           EnsureNonNull(url.Host().ToString()),
                                           port));
}

SecurityOrigin::SecurityOrigin(const String& protocol,
                               const String& host,
                               uint16_t port)
    : protocol_(protocol), host_(host), domain_(host_), port_(port) {
  DCHECK(url::SchemeHostPort(protocol.Utf8(), host.Utf8(), port,
                             url::SchemeHostPort::CHECK_CANONICALIZATION)
             .IsValid());
  DCHECK(!IsOpaque());
  // By default, only local SecurityOrigins can load local resources.
  can_load_local_resources_ = IsLocal();
}

SecurityOrigin::SecurityOrigin(const url::Origin::Nonce& nonce,
                               const SecurityOrigin* precursor)
    : nonce_if_opaque_(nonce), precursor_origin_(precursor) {}

SecurityOrigin::SecurityOrigin(NewUniqueOpaque, const SecurityOrigin* precursor)
    : nonce_if_opaque_(std::in_place), precursor_origin_(precursor) {}

SecurityOrigin::SecurityOrigin(const SecurityOrigin* other,
                               ConstructIsolatedCopy)
    : protocol_(other->protocol_),
      host_(other->host_),
      domain_(other->domain_),
      port_(other->port_),
      nonce_if_opaque_(other->nonce_if_opaque_),
      universal_access_(other->universal_access_),
      domain_was_set_in_dom_(other->domain_was_set_in_dom_),
      can_load_local_resources_(other->can_load_local_resources_),
      block_local_access_from_local_origin_(
          other->block_local_access_from_local_origin_),
      is_opaque_origin_potentially_trustworthy_(
          other->is_opaque_origin_potentially_trustworthy_),
      cross_agent_cluster_access_(other->cross_agent_cluster_access_),
      agent_cluster_id_(other->agent_cluster_id_),
      precursor_origin_(other->precursor_origin_
                            ? other->precursor_origin_->IsolatedCopy()
                            : nullptr) {}

SecurityOrigin::SecurityOrigin(const SecurityOrigin* other,
                               ConstructSameThreadCopy)
    : protocol_(other->protocol_),
      host_(other->host_),
      domain_(other->domain_),
      port_(other->port_),
      nonce_if_opaque_(other->nonce_if_opaque_),
      universal_access_(other->universal_access_),
      domain_was_set_in_dom_(other->domain_was_set_in_dom_),
      can_load_local_resources_(other->can_load_local_resources_),
      block_local_access_from_local_origin_(
          other->block_local_access_from_local_origin_),
      is_opaque_origin_potentially_trustworthy_(
          other->is_opaque_origin_potentially_trustworthy_),
      cross_agent_cluster_access_(other->cross_agent_cluster_access_),
      agent_cluster_id_(other->agent_cluster_id_),
      precursor_origin_(other->precursor_origin_) {}

scoped_refptr<SecurityOrigin> SecurityOrigin::CreateWithReferenceOrigin(
    const KURL& url,
    const SecurityOrigin* reference_origin) {
  if (url.ProtocolIs("blob") && BlobURL::GetOrigin(url) == "null") {
    if (scoped_refptr<SecurityOrigin> origin =
            BlobURLNullOriginMap::GetInstance()->Get(url))
      return origin;
  }

  if (url.IsAboutBlankURL()) {
    if (!reference_origin)
      return CreateUniqueOpaque();
    return reference_origin->IsolatedCopy();
  }

  if (ShouldTreatAsOpaqueOrigin(url)) {
    if (!reference_origin)
      return CreateUniqueOpaque();
    return reference_origin->DeriveNewOpaqueOrigin();
  }

  if (ShouldUseInnerURL(url))
    return CreateInternal(ExtractInnerURL(url));

  return CreateInternal(url);
}

scoped_refptr<SecurityOrigin> SecurityOrigin::Create(const KURL& url) {
  return CreateWithReferenceOrigin(url, nullptr);
}

scoped_refptr<SecurityOrigin> SecurityOrigin::CreateUniqueOpaque() {
  scoped_refptr<SecurityOrigin> origin = base::AdoptRef(
      new SecurityOrigin(NewUniqueOpaque::kWithLazyInitNonce, nullptr));
  DCHECK(origin->IsOpaque());
  DCHECK(!origin->precursor_origin_);
  return origin;
}

scoped_refptr<SecurityOrigin> SecurityOrigin::CreateOpaque(
    const url::Origin::Nonce& nonce,
    const SecurityOrigin* precursor) {
  scoped_refptr<SecurityOrigin> origin =
      base::AdoptRef(new SecurityOrigin(nonce, precursor));
  DCHECK(origin->IsOpaque());
  return origin;
}

scoped_refptr<SecurityOrigin> SecurityOrigin::CreateFromUrlOrigin(
    const url::Origin& origin) {
  const url::SchemeHostPort& tuple = origin.GetTupleOrPrecursorTupleIfOpaque();
  DCHECK(String::FromUTF8(tuple.scheme()).ContainsOnlyASCIIOrEmpty());
  DCHECK(String::FromUTF8(tuple.host()).ContainsOnlyASCIIOrEmpty());

  scoped_refptr<SecurityOrigin> tuple_origin;
  if (tuple.IsValid()) {
    tuple_origin =
        CreateFromValidTuple(String::FromUTF8(tuple.scheme()),
                             String::FromUTF8(tuple.host()), tuple.port());
  }
  const base::UnguessableToken* nonce_if_opaque =
      origin.GetNonceForSerialization();
  DCHECK_EQ(!!nonce_if_opaque, origin.opaque());
  if (nonce_if_opaque) {
    return base::AdoptRef(new SecurityOrigin(
        url::Origin::Nonce(*nonce_if_opaque), tuple_origin.get()));
  }
  CHECK(tuple_origin);
  return tuple_origin;
}

url::Origin SecurityOrigin::ToUrlOrigin() const {
  const SecurityOrigin* unmasked = GetOriginOrPrecursorOriginIfOpaque();
  std::string scheme = unmasked->protocol_.Utf8();
  std::string host = unmasked->host_.Utf8();
  uint16_t port = unmasked->port_;
  if (nonce_if_opaque_) {
    url::Origin result = url::Origin::CreateOpaqueFromNormalizedPrecursorTuple(
        std::move(scheme), std::move(host), port, *nonce_if_opaque_);
    CHECK(result.opaque());
    return result;
  }
  url::Origin result = url::Origin::CreateFromNormalizedTuple(
      std::move(scheme), std::move(host), port);
  CHECK(!result.opaque());
  return result;
}

scoped_refptr<SecurityOrigin> SecurityOrigin::IsolatedCopy() const {
  return base::AdoptRef(new SecurityOrigin(
      this, ConstructIsolatedCopy::kConstructIsolatedCopyBit));
}

void SecurityOrigin::SetDomainFromDOM(const String& new_domain) {
  domain_was_set_in_dom_ = true;
  domain_ = new_domain;
}

String SecurityOrigin::RegistrableDomain() const {
  if (IsOpaque())
    return String();

  OriginAccessEntry entry(
      *this, network::mojom::CorsDomainMatchMode::kAllowRegistrableDomains);
  String domain = entry.registrable_domain();
  return domain.empty() ? String() : domain;
}

const base::UnguessableToken* SecurityOrigin::GetNonceForSerialization() const {
  // The call to token() forces initialization of the |nonce_if_opaque_| if
  // not already initialized.
  return nonce_if_opaque_ ? &nonce_if_opaque_->token() : nullptr;
}

bool SecurityOrigin::CanAccess(const SecurityOrigin* other,
                               AccessResultDomainDetail& detail) const {
  if (universal_access_) {
    detail = AccessResultDomainDetail::kDomainNotRelevant;
    return true;
  }

  bool can_access = IsSameOriginDomainWith(other, detail);

  // Compare that the clusters are the same.
  if (can_access && !cross_agent_cluster_access_ &&
      !agent_cluster_id_.is_empty() && !other->agent_cluster_id_.is_empty() &&
      agent_cluster_id_ != other->agent_cluster_id_) {
    detail = AccessResultDomainDetail::kDomainNotRelevantAgentClusterMismatch;
    can_access = false;
  }

  return can_access;
}

bool SecurityOrigin::PassesFileCheck(const SecurityOrigin* other) const {
  DCHECK(IsLocal());
  DCHECK(other->IsLocal());

  return !block_local_access_from_local_origin_ &&
         !other->block_local_access_from_local_origin_;
}

bool SecurityOrigin::CanRequest(const KURL& url) const {
  if (universal_access_)
    return true;

  if (SerializesAsNull()) {
    // Allow the request if the URL is blob and it has the same "null" origin
    // with |this|.
    if (!url.ProtocolIs("blob") || BlobURL::GetOrigin(url) != "null")
      return false;
    if (BlobURLNullOriginMap::GetInstance()->Get(url) == this)
      return true;
    // BlobURLNullOriginMap doesn't work for cross-thread blob URL loading
    // (e.g., top-level worker script loading) because SecurityOrigin and
    // BlobURLNullOriginMap are thread-specific. For the case, check
    // BlobURLOpaqueOriginNonceMap.
    const base::UnguessableToken* nonce = GetNonceForSerialization();
    if (nonce && BlobURLOpaqueOriginNonceMap::GetInstance().Get(url) == *nonce)
      return true;
    return false;
  }

  scoped_refptr<const SecurityOrigin> target_origin =
      SecurityOrigin::Create(url);

  if (target_origin->IsOpaque())
    return false;

  // We call IsSameOriginWith here instead of canAccess because we want to
  // ignore `document.domain` effects.
  if (IsSameOriginWith(target_origin.get()))
    return true;

  if (SecurityPolicy::IsOriginAccessAllowed(this, target_origin.get()))
    return true;

  return false;
}

bool SecurityOrigin::CanReadContent(const KURL& url) const {
  if (CanRequest(url))
    return true;

  // This function exists because we treat data URLs as having a unique opaque
  // origin, see https://fetch.spec.whatwg.org/#main-fetch.
  // TODO(dcheng): If we plumb around the 'precursor' origin, then maybe we
  // don't need this?
  if (url.ProtocolIsData())
    return true;

  return false;
}

bool SecurityOrigin::CanDisplay(const KURL& url) const {
  if (universal_access_)
    return true;

  // Data URLs can always be displayed.
  if (base::FeatureList::IsEnabled(features::kOptimizeLoadingDataUrls) &&
      url.ProtocolIsData()) {
    return true;
  }

  String protocol = url.Protocol();
  if (SchemeRegistry::CanDisplayOnlyIfCanRequest(protocol))
    return CanRequest(url);

  if (SchemeRegistry::ShouldTreatURLSchemeAsDisplayIsolated(protocol)) {
    return protocol_ == protocol ||
           SecurityPolicy::IsOriginAccessToURLAllowed(this, url);
  }

  if (base::Contains(url::GetLocalSchemes(), protocol.Ascii())) {
    return CanLoadLocalResources() ||
           SecurityPolicy::IsOriginAccessToURLAllowed(this, url);
  }

  return true;
}

bool SecurityOrigin::IsPotentiallyTrustworthy() const {
  // TODO(https://crbug.com/1153336): The code below can hopefully be eventually
  // deleted and IsOriginPotentiallyTrustworthy can be used instead (from
  // //services/network/public/cpp/is_potentially_trustworthy.h).

  DCHECK_NE(protocol_, "data");
  if (IsOpaque())
    return is_opaque_origin_potentially_trustworthy_;
  return network::IsOriginPotentiallyTrustworthy(ToUrlOrigin());
}

// static
String SecurityOrigin::IsPotentiallyTrustworthyErrorMessage() {
  return "Only secure origins are allowed (see: https://goo.gl/Y0ZkNV).";
}

void SecurityOrigin::GrantLoadLocalResources() {
  // Granting privileges to some, but not all, documents in a SecurityOrigin
  // is a security hazard because the documents without the privilege can
  // obtain the privilege by injecting script into the documents that have
  // been granted the privilege.
  can_load_local_resources_ = true;
}

void SecurityOrigin::GrantUniversalAccess() {
  universal_access_ = true;
}

void SecurityOrigin::GrantCrossAgentClusterAccess() {
  cross_agent_cluster_access_ = true;
}

void SecurityOrigin::BlockLocalAccessFromLocalOrigin() {
  DCHECK(IsLocal());
  block_local_access_from_local_origin_ = true;
}

bool SecurityOrigin::IsLocal() const {
  return base::Contains(url::GetLocalSchemes(), protocol_.Ascii());
}

bool SecurityOrigin::IsLocalhost() const {
  // We special-case "[::1]" here because `net::HostStringIsLocalhost` expects a
  // canonicalization that excludes the braces; a simple string comparison is
  // simpler than trying to adjust Blink's canonicalization.
  return host_ == "[::1]" || net::HostStringIsLocalhost(host_.Ascii());
}

String SecurityOrigin::ToString() const {
  if (SerializesAsNull())
    return "null";
  return ToRawString();
}

AtomicString SecurityOrigin::ToAtomicString() const {
  if (SerializesAsNull())
    return AtomicString("null");

  if (protocol_ == "file")
    return AtomicString("file://");

  StringBuilder result;
  BuildRawString(result);
  return result.ToAtomicString();
}

String SecurityOrigin::ToRawString() const {
  if (protocol_ == "file")
    return "file://";

  StringBuilder result;
  BuildRawString(result);
  return result.ToString();
}

void SecurityOrigin::BuildRawString(StringBuilder& builder) const {
  builder.Append(protocol_);
  builder.Append("://");
  builder.Append(host_);

  if (DefaultPortForProtocol(protocol_) &&
      port_ != DefaultPortForProtocol(protocol_)) {
    builder.Append(':');
    builder.AppendNumber(port_);
  }
}

String SecurityOrigin::ToTokenForFastCheck() const {
  CHECK(!agent_cluster_id_.is_empty());
  if (SerializesAsNull())
    return String();

  StringBuilder result;
  BuildRawString(result);
  // Append the agent cluster id to the generated token to prevent
  // access from two contexts that have the same origin but are
  // in different agent clusters.
  result.Append(agent_cluster_id_.ToString().c_str());
  return result.ToString();
}

scoped_refptr<SecurityOrigin> SecurityOrigin::CreateFromString(
    const String& origin_string) {
  return SecurityOrigin::Create(KURL(NullURL(), origin_string));
}

scoped_refptr<SecurityOrigin> SecurityOrigin::CreateFromValidTuple(
    const String& protocol,
    const String& host,
    uint16_t port) {
  return base::AdoptRef(new SecurityOrigin(protocol, host, port));
}

bool SecurityOrigin::IsSameOriginWith(const SecurityOrigin* other) const {
  // This is needed to ensure a local origin considered to have the same scheme,
  // host, and port to itself.
  // TODO(tzik): Make the local origin unique but not opaque, and remove this
  // condition.
  if (this == other)
    return true;

  if (IsOpaque() || other->IsOpaque())
    return nonce_if_opaque_ == other->nonce_if_opaque_;

  if (host_ != other->host_)
    return false;

  if (protocol_ != other->protocol_)
    return false;

  if (port_ != other->port_)
    return false;

  if (IsLocal() && !PassesFileCheck(other))
    return false;

  return true;
}

bool SecurityOrigin::AreSameOrigin(const KURL& a, const KURL& b) {
  scoped_refptr<const SecurityOrigin> origin_a = SecurityOrigin::Create(a);
  scoped_refptr<const SecurityOrigin> origin_b = SecurityOrigin::Create(b);
  return origin_b->IsSameOriginWith(origin_a.get());
}

bool SecurityOrigin::IsSameOriginDomainWith(
    const SecurityOrigin* other,
    AccessResultDomainDetail& detail) const {
  // This is needed to ensure an origin can access to itself under nullified
  // document.domain.
  // TODO(tzik): Update the nulled domain handling and remove this condition.
  if (this == other) {
    detail = AccessResultDomainDetail::kDomainNotRelevant;
    return true;
  }

  if (IsOpaque() || other->IsOpaque()) {
    detail = AccessResultDomainDetail::kDomainNotRelevant;
    return nonce_if_opaque_ == other->nonce_if_opaque_;
  }

  // document.domain handling, as per
  // https://html.spec.whatwg.org/C/#dom-document-domain:
  //
  // 1) Neither document has set document.domain. In this case, we insist
  //    that the scheme, host, and port of the URLs match.
  //
  // 2) Both documents have set document.domain. In this case, we insist
  //    that the documents have set document.domain to the same value and
  //    that the scheme of the URLs match. Ports do not need to match.
  bool can_access = false;
  if (protocol_ == other->protocol_) {
    if (!domain_was_set_in_dom_ && !other->domain_was_set_in_dom_) {
      detail = AccessResultDomainDetail::kDomainNotSet;
      if (host_ == other->host_ && port_ == other->port_)
        can_access = true;
    } else if (domain_was_set_in_dom_ && other->domain_was_set_in_dom_) {
      if (domain_ == other->domain_) {
        can_access = true;
        detail = (host_ == other->host_ && port_ == other->port_)
                     ? AccessResultDomainDetail::kDomainMatchUnnecessary
                     : AccessResultDomainDetail::kDomainMatchNecessary;
      } else {
        detail = (host_ == other->host_ && port_ == other->port_)
                     ? AccessResultDomainDetail::kDomainMismatch
                     : AccessResultDomainDetail::kDomainNotRelevant;
      }
    } else {
      detail = (host_ == other->host_ && port_ == other->port_)
                   ? AccessResultDomainDetail::kDomainSetByOnlyOneOrigin
                   : AccessResultDomainDetail::kDomainNotRelevant;
    }
  } else {
    detail = AccessResultDomainDetail::kDomainNotRelevant;
  }

  if (can_access && IsLocal() && !PassesFileCheck(other)) {
    detail = AccessResultDomainDetail::kDomainNotRelevant;
    can_access = false;
  }

  return can_access;
}

bool SecurityOrigin::IsSameSiteWith(const SecurityOrigin* other) const {
  // "A and B are either both opaque origins, or both tuple origins with the
  // same scheme"
  if (IsOpaque() != other->IsOpaque())
    return false;
  if (!IsOpaque() && Protocol() != other->Protocol())
    return false;

  // Schemelessly same site check.
  // https://html.spec.whatwg.org/#schemelessly-same-site
  if (IsOpaque())
    return IsSameOriginWith(other);
  String registrable_domain = RegistrableDomain();
  if (registrable_domain.IsNull()) {
    return Host() == other->Host();
  }
  return registrable_domain == other->RegistrableDomain();
}

const KURL& SecurityOrigin::UrlWithUniqueOpaqueOrigin() {
  DCHECK(IsMainThread());
  DEFINE_STATIC_LOCAL(const KURL, url, ("data:,"));
  return url;
}

std::unique_ptr<SecurityOrigin::PrivilegeData>
SecurityOrigin::CreatePrivilegeData() const {
  std::unique_ptr<PrivilegeData> privilege_data =
      std::make_unique<PrivilegeData>();
  privilege_data->universal_access_ = universal_access_;
  privilege_data->can_load_local_resources_ = can_load_local_resources_;
  privilege_data->block_local_access_from_local_origin_ =
      block_local_access_from_local_origin_;
  return privilege_data;
}

void SecurityOrigin::TransferPrivilegesFrom(
    std::unique_ptr<PrivilegeData> privilege_data) {
  universal_access_ = privilege_data->universal_access_;
  can_load_local_resources_ = privilege_data->can_load_local_resources_;
  block_local_access_from_local_origin_ =
      privilege_data->block_local_access_from_local_origin_;
}

void SecurityOrigin::SetOpaqueOriginIsPotentiallyTrustworthy(
    bool is_opaque_origin_potentially_trustworthy) {
  DCHECK(!is_opaque_origin_potentially_trustworthy || IsOpaque());
  is_opaque_origin_potentially_trustworthy_ =
      is_opaque_origin_potentially_trustworthy;
}

scoped_refptr<SecurityOrigin> SecurityOrigin::DeriveNewOpaqueOrigin() const {
  return base::AdoptRef(
      new SecurityOrigin(NewUniqueOpaque::kWithLazyInitNonce,
                         GetOriginOrPrecursorOriginIfOpaque()));
}

const SecurityOrigin* SecurityOrigin::GetOriginOrPrecursorOriginIfOpaque()
    const {
  if (!precursor_origin_)
    return this;

  DCHECK(IsOpaque());
  return precursor_origin_.get();
}

String SecurityOrigin::CanonicalizeSpecialHost(const String& host,
                                               bool* success) {
  url::Component out_host;
  url::RawCanonOutputT<char> canon_output;
  if (host.Is8Bit()) {
    StringUTF8Adaptor utf8(host);
    *success = url::CanonicalizeSpecialHost(
        utf8.data(), url::Component(0, utf8.size()), canon_output, out_host);
  } else {
    *success = url::CanonicalizeSpecialHost(host.Characters16(),
                                            url::Component(0, host.length()),
                                            canon_output, out_host);
  }
  return String::FromUTF8(canon_output.view());
}

String SecurityOrigin::CanonicalizeHost(const String& host,
                                        const String& scheme,
                                        bool* success) {
  if (scheme != url::kFileScheme) {
    return CanonicalizeSpecialHost(host, success);
  }

  url::Component out_host;
  url::RawCanonOutputT<char> canon_output;
  if (host.Is8Bit()) {
    StringUTF8Adaptor utf8(host);
    *success = url::CanonicalizeFileHost(
        utf8.data(), url::Component(0, utf8.size()), canon_output, out_host);
  } else {
    *success = url::CanonicalizeFileHost(host.Characters16(),
                                         url::Component(0, host.length()),
                                         canon_output, out_host);
  }
  return String::FromUTF8(canon_output.data(), canon_output.length());
}

scoped_refptr<SecurityOrigin> SecurityOrigin::GetOriginForAgentCluster(
    const base::UnguessableToken& agent_cluster_id) {
  if (agent_cluster_id_ == agent_cluster_id)
    return this;
  auto result = base::AdoptRef(new SecurityOrigin(
      this, ConstructSameThreadCopy::kConstructSameThreadCopyBit));
  result->agent_cluster_id_ = agent_cluster_id;
  return result;
}

bool SecurityOrigin::SerializesAsNull() const {
  if (IsOpaque())
    return true;

  if (IsLocal() && block_local_access_from_local_origin_)
    return true;

  return false;
}

}  // namespace blink
