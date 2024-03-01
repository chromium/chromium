// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/net_ipc_param_traits.h"

#include <string_view>

#include "ipc/ipc_message_utils.h"
#include "ipc/ipc_mojo_param_traits.h"
#include "ipc/ipc_platform_file.h"
#include "net/cert/cert_verify_result.h"
#include "net/http/http_util.h"

namespace IPC {

void ParamTraits<net::AuthCredentials>::Write(base::Pickle* m,
                                              const param_type& p) {
  WriteParam(m, p.username());
  WriteParam(m, p.password());
}

bool ParamTraits<net::AuthCredentials>::Read(const base::Pickle* m,
                                             base::PickleIterator* iter,
                                             param_type* r) {
  std::u16string username;
  bool read_username = ReadParam(m, iter, &username);
  std::u16string password;
  bool read_password = ReadParam(m, iter, &password);

  if (!read_username || !read_password)
    return false;

  r->Set(username, password);
  return true;
}

void ParamTraits<net::AuthCredentials>::Log(const param_type& p,
                                            std::string* l) {
  l->append("<AuthCredentials>");
}

void ParamTraits<net::CertVerifyResult>::Write(base::Pickle* m,
                                               const param_type& p) {
  WriteParam(m, p.verified_cert);
  WriteParam(m, p.cert_status);
  WriteParam(m, p.has_sha1);
  WriteParam(m, p.public_key_hashes);
  WriteParam(m, p.is_issued_by_known_root);
  WriteParam(m, p.is_issued_by_additional_trust_anchor);
  WriteParam(m, p.ocsp_result);
  WriteParam(m, p.scts);
  WriteParam(m, p.policy_compliance);
}

bool ParamTraits<net::CertVerifyResult>::Read(const base::Pickle* m,
                                              base::PickleIterator* iter,
                                              param_type* r) {
  return ReadParam(m, iter, &r->verified_cert) &&
         ReadParam(m, iter, &r->cert_status) &&
         ReadParam(m, iter, &r->has_sha1) &&
         ReadParam(m, iter, &r->public_key_hashes) &&
         ReadParam(m, iter, &r->is_issued_by_known_root) &&
         ReadParam(m, iter, &r->is_issued_by_additional_trust_anchor) &&
         ReadParam(m, iter, &r->ocsp_result) && ReadParam(m, iter, &r->scts) &&
         ReadParam(m, iter, &r->policy_compliance);
}

void ParamTraits<net::CertVerifyResult>::Log(const param_type& p,
                                             std::string* l) {
  l->append("<CertVerifyResult>");
}

void ParamTraits<net::HashValue>::Write(base::Pickle* m, const param_type& p) {
  WriteParam(m, p.ToString());
}

bool ParamTraits<net::HashValue>::Read(const base::Pickle* m,
                                       base::PickleIterator* iter,
                                       param_type* r) {
  std::string_view encoded;
  return iter->ReadStringPiece(&encoded) && r->FromString(encoded);
}

void ParamTraits<net::HashValue>::Log(const param_type& p, std::string* l) {
  l->append("<HashValue>");
}

void ParamTraits<net::IPEndPoint>::Write(base::Pickle* m, const param_type& p) {
  WriteParam(m, p.address());
  WriteParam(m, p.port());
}

bool ParamTraits<net::IPEndPoint>::Read(const base::Pickle* m,
                                        base::PickleIterator* iter,
                                        param_type* p) {
  net::IPAddress address;
  uint16_t port;
  if (!ReadParam(m, iter, &address) || !ReadParam(m, iter, &port))
    return false;

  *p = net::IPEndPoint(address, port);
  return true;
}

void ParamTraits<net::IPEndPoint>::Log(const param_type& p, std::string* l) {
  LogParam("IPEndPoint:" + p.ToString(), l);
}

void ParamTraits<net::IPAddress>::Write(base::Pickle* m, const param_type& p) {
  absl::InlinedVector<uint8_t, 16> bytes;
  for (uint8_t byte : p.bytes())
    bytes.push_back(byte);
  WriteParam(m, bytes);
}

bool ParamTraits<net::IPAddress>::Read(const base::Pickle* m,
                                       base::PickleIterator* iter,
                                       param_type* p) {
  absl::InlinedVector<uint8_t, 16> bytes;
  if (!ReadParam(m, iter, &bytes))
    return false;
  if (bytes.size() > 16) {
    return false;
  }
  *p = net::IPAddress(bytes);
  return true;
}

void ParamTraits<net::IPAddress>::Log(const param_type& p, std::string* l) {
  LogParam("IPAddress:" + (p.empty() ? "(empty)" : p.ToString()), l);
}

void ParamTraits<net::HttpRequestHeaders>::Write(base::Pickle* m,
                                                 const param_type& p) {
  WriteParam(m, static_cast<int>(p.GetHeaderVector().size()));
  for (size_t i = 0; i < p.GetHeaderVector().size(); ++i)
    WriteParam(m, p.GetHeaderVector()[i]);
}

bool ParamTraits<net::HttpRequestHeaders>::Read(const base::Pickle* m,
                                                base::PickleIterator* iter,
                                                param_type* r) {
  // Sanity check.
  size_t size;
  if (!iter->ReadLength(&size))
    return false;
  for (size_t i = 0; i < size; ++i) {
    net::HttpRequestHeaders::HeaderKeyValuePair pair;
    if (!ReadParam(m, iter, &pair) ||
        !net::HttpUtil::IsValidHeaderName(pair.key) ||
        !net::HttpUtil::IsValidHeaderValue(pair.value))
      return false;
    r->SetHeader(pair.key, pair.value);
  }
  return true;
}

void ParamTraits<net::HttpRequestHeaders>::Log(const param_type& p,
                                               std::string* l) {
  l->append(p.ToString());
}

void ParamTraits<scoped_refptr<net::HttpResponseHeaders>>::Write(
    base::Pickle* m,
    const param_type& p) {
  WriteParam(m, p.get() != nullptr);
  if (p.get()) {
    // Do not disclose Set-Cookie headers over IPC.
    p->Persist(m, net::HttpResponseHeaders::PERSIST_SANS_COOKIES);
  }
}

bool ParamTraits<scoped_refptr<net::HttpResponseHeaders>>::Read(
    const base::Pickle* m,
    base::PickleIterator* iter,
    param_type* r) {
  bool has_object;
  if (!ReadParam(m, iter, &has_object))
    return false;
  if (has_object)
    *r = base::MakeRefCounted<net::HttpResponseHeaders>(iter);
  return true;
}

void ParamTraits<scoped_refptr<net::HttpResponseHeaders>>::Log(
    const param_type& p,
    std::string* l) {
  l->append("<HttpResponseHeaders>");
}

void ParamTraits<bssl::OCSPVerifyResult>::Write(base::Pickle* m,
                                                const param_type& p) {
  WriteParam(m, p.response_status);
  WriteParam(m, p.revocation_status);
}

bool ParamTraits<bssl::OCSPVerifyResult>::Read(const base::Pickle* m,
                                               base::PickleIterator* iter,
                                               param_type* r) {
  return ReadParam(m, iter, &r->response_status) &&
         ReadParam(m, iter, &r->revocation_status);
}

void ParamTraits<bssl::OCSPVerifyResult>::Log(const param_type& p,
                                              std::string* l) {
  l->append("<OCSPVerifyResult>");
}

void ParamTraits<net::ResolveErrorInfo>::Write(base::Pickle* m,
                                               const param_type& p) {
  WriteParam(m, p.error);
  WriteParam(m, p.is_secure_network_error);
}
bool ParamTraits<net::ResolveErrorInfo>::Read(const base::Pickle* m,
                                              base::PickleIterator* iter,
                                              param_type* r) {
  return ReadParam(m, iter, &r->error) &&
         ReadParam(m, iter, &r->is_secure_network_error);
}
void ParamTraits<net::ResolveErrorInfo>::Log(const param_type& p,
                                             std::string* l) {
  l->append("<ResolveErrorInfo>");
}

void ParamTraits<net::SSLInfo>::Write(base::Pickle* m, const param_type& p) {
  WriteParam(m, p.is_valid());
  if (!p.is_valid())
    return;
  WriteParam(m, p.cert);
  WriteParam(m, p.unverified_cert);
  WriteParam(m, p.cert_status);
  WriteParam(m, p.key_exchange_group);
  WriteParam(m, p.peer_signature_algorithm);
  WriteParam(m, p.connection_status);
  WriteParam(m, p.is_issued_by_known_root);
  WriteParam(m, p.pkp_bypassed);
  WriteParam(m, p.client_cert_sent);
  WriteParam(m, p.encrypted_client_hello);
  WriteParam(m, p.handshake_type);
  WriteParam(m, p.public_key_hashes);
  WriteParam(m, p.signed_certificate_timestamps);
  WriteParam(m, p.ct_policy_compliance);
  WriteParam(m, p.ocsp_result);
  WriteParam(m, p.is_fatal_cert_error);
}

bool ParamTraits<net::SSLInfo>::Read(const base::Pickle* m,
                                     base::PickleIterator* iter,
                                     param_type* r) {
  bool is_valid = false;
  if (!ReadParam(m, iter, &is_valid))
    return false;
  if (!is_valid)
    return true;
  return ReadParam(m, iter, &r->cert) &&
         ReadParam(m, iter, &r->unverified_cert) &&
         ReadParam(m, iter, &r->cert_status) &&
         ReadParam(m, iter, &r->key_exchange_group) &&
         ReadParam(m, iter, &r->peer_signature_algorithm) &&
         ReadParam(m, iter, &r->connection_status) &&
         ReadParam(m, iter, &r->is_issued_by_known_root) &&
         ReadParam(m, iter, &r->pkp_bypassed) &&
         ReadParam(m, iter, &r->client_cert_sent) &&
         ReadParam(m, iter, &r->encrypted_client_hello) &&
         ReadParam(m, iter, &r->handshake_type) &&
         ReadParam(m, iter, &r->public_key_hashes) &&
         ReadParam(m, iter, &r->signed_certificate_timestamps) &&
         ReadParam(m, iter, &r->ct_policy_compliance) &&
         ReadParam(m, iter, &r->ocsp_result) &&
         ReadParam(m, iter, &r->is_fatal_cert_error);
}

void ParamTraits<net::SSLInfo>::Log(const param_type& p, std::string* l) {
  l->append("<SSLInfo>");
}

void ParamTraits<scoped_refptr<net::ct::SignedCertificateTimestamp>>::Write(
    base::Pickle* m,
    const param_type& p) {
  WriteParam(m, p.get() != nullptr);
  if (p.get())
    p->Persist(m);
}

bool ParamTraits<scoped_refptr<net::ct::SignedCertificateTimestamp>>::Read(
    const base::Pickle* m,
    base::PickleIterator* iter,
    param_type* r) {
  bool has_object;
  if (!ReadParam(m, iter, &has_object))
    return false;
  if (has_object)
    *r = net::ct::SignedCertificateTimestamp::CreateFromPickle(iter);
  return true;
}

void ParamTraits<scoped_refptr<net::ct::SignedCertificateTimestamp>>::Log(
    const param_type& p,
    std::string* l) {
  l->append("<SignedCertificateTimestamp>");
}

void ParamTraits<scoped_refptr<net::X509Certificate>>::Write(
    base::Pickle* m,
    const param_type& p) {
  WriteParam(m, !!p);
  if (p)
    p->Persist(m);
}

bool ParamTraits<scoped_refptr<net::X509Certificate>>::Read(
    const base::Pickle* m,
    base::PickleIterator* iter,
    param_type* r) {
  DCHECK(!*r);
  bool has_object;
  if (!ReadParam(m, iter, &has_object))
    return false;
  if (!has_object)
    return true;
  net::X509Certificate::UnsafeCreateOptions options;
  // Setting the |printable_string_is_utf8| option to be true here is necessary
  // to round-trip any X509Certificate objects that were parsed with this
  // option in the first place.
  // See https://crbug.com/770323 and https://crbug.com/788655.
  options.printable_string_is_utf8 = true;
  *r = net::X509Certificate::CreateFromPickleUnsafeOptions(iter, options);
  return !!r->get();
}

void ParamTraits<scoped_refptr<net::X509Certificate>>::Log(const param_type& p,
                                                           std::string* l) {
  l->append("<X509Certificate>");
}

void ParamTraits<net::LoadTimingInfo>::Write(base::Pickle* m,
                                             const param_type& p) {
  WriteParam(m, p.socket_log_id);
  WriteParam(m, p.socket_reused);
  WriteParam(m, p.request_start_time.is_null());
  if (p.request_start_time.is_null())
    return;
  WriteParam(m, p.request_start_time);
  WriteParam(m, p.request_start);
  WriteParam(m, p.proxy_resolve_start);
  WriteParam(m, p.proxy_resolve_end);
  WriteParam(m, p.connect_timing.domain_lookup_start);
  WriteParam(m, p.connect_timing.domain_lookup_end);
  WriteParam(m, p.connect_timing.connect_start);
  WriteParam(m, p.connect_timing.connect_end);
  WriteParam(m, p.connect_timing.ssl_start);
  WriteParam(m, p.connect_timing.ssl_end);
  WriteParam(m, p.send_start);
  WriteParam(m, p.send_end);
  WriteParam(m, p.receive_headers_start);
  WriteParam(m, p.receive_headers_end);
  WriteParam(m, p.receive_non_informational_headers_start);
  WriteParam(m, p.first_early_hints_time);
  WriteParam(m, p.push_start);
  WriteParam(m, p.push_end);
}

bool ParamTraits<net::LoadTimingInfo>::Read(const base::Pickle* m,
                                            base::PickleIterator* iter,
                                            param_type* r) {
  bool has_no_times;
  if (!ReadParam(m, iter, &r->socket_log_id) ||
      !ReadParam(m, iter, &r->socket_reused) ||
      !ReadParam(m, iter, &has_no_times)) {
    return false;
  }
  if (has_no_times)
    return true;

  return ReadParam(m, iter, &r->request_start_time) &&
         ReadParam(m, iter, &r->request_start) &&
         ReadParam(m, iter, &r->proxy_resolve_start) &&
         ReadParam(m, iter, &r->proxy_resolve_end) &&
         ReadParam(m, iter, &r->connect_timing.domain_lookup_start) &&
         ReadParam(m, iter, &r->connect_timing.domain_lookup_end) &&
         ReadParam(m, iter, &r->connect_timing.connect_start) &&
         ReadParam(m, iter, &r->connect_timing.connect_end) &&
         ReadParam(m, iter, &r->connect_timing.ssl_start) &&
         ReadParam(m, iter, &r->connect_timing.ssl_end) &&
         ReadParam(m, iter, &r->send_start) &&
         ReadParam(m, iter, &r->send_end) &&
         ReadParam(m, iter, &r->receive_headers_start) &&
         ReadParam(m, iter, &r->receive_headers_end) &&
         ReadParam(m, iter, &r->receive_non_informational_headers_start) &&
         ReadParam(m, iter, &r->first_early_hints_time) &&
         ReadParam(m, iter, &r->push_start) && ReadParam(m, iter, &r->push_end);
}

void ParamTraits<net::LoadTimingInfo>::Log(const param_type& p,
                                           std::string* l) {
  l->append("(");
  LogParam(p.socket_log_id, l);
  l->append(",");
  LogParam(p.socket_reused, l);
  l->append(",");
  LogParam(p.request_start_time, l);
  l->append(", ");
  LogParam(p.request_start, l);
  l->append(", ");
  LogParam(p.proxy_resolve_start, l);
  l->append(", ");
  LogParam(p.proxy_resolve_end, l);
  l->append(", ");
  LogParam(p.connect_timing.domain_lookup_start, l);
  l->append(", ");
  LogParam(p.connect_timing.domain_lookup_end, l);
  l->append(", ");
  LogParam(p.connect_timing.connect_start, l);
  l->append(", ");
  LogParam(p.connect_timing.connect_end, l);
  l->append(", ");
  LogParam(p.connect_timing.ssl_start, l);
  l->append(", ");
  LogParam(p.connect_timing.ssl_end, l);
  l->append(", ");
  LogParam(p.send_start, l);
  l->append(", ");
  LogParam(p.send_end, l);
  l->append(", ");
  LogParam(p.receive_headers_start, l);
  l->append(", ");
  LogParam(p.receive_headers_end, l);
  l->append(", ");
  LogParam(p.receive_non_informational_headers_start, l);
  l->append(", ");
  LogParam(p.first_early_hints_time, l);
  l->append(", ");
  LogParam(p.push_start, l);
  l->append(", ");
  LogParam(p.push_end, l);
  l->append(")");
}

void ParamTraits<net::SiteForCookies>::Write(base::Pickle* m,
                                             const param_type& p) {
  WriteParam(m, p.site());
  WriteParam(m, p.schemefully_same());
}

bool ParamTraits<net::SiteForCookies>::Read(const base::Pickle* m,
                                            base::PickleIterator* iter,
                                            param_type* r) {
  net::SchemefulSite site;
  bool schemefully_same;
  if (!ReadParam(m, iter, &site) || !ReadParam(m, iter, &schemefully_same))
    return false;

  return net::SiteForCookies::FromWire(site, schemefully_same, r);
}

void ParamTraits<net::SiteForCookies>::Log(const param_type& p,
                                           std::string* l) {
  l->append("(");
  LogParam(p.scheme(), l);
  l->append(",");
  LogParam(p.registrable_domain(), l);
  l->append(",");
  LogParam(p.schemefully_same(), l);
  l->append(")");
}

void ParamTraits<url::Origin>::Write(base::Pickle* m, const url::Origin& p) {
  WriteParam(m, p.GetTupleOrPrecursorTupleIfOpaque().scheme());
  WriteParam(m, p.GetTupleOrPrecursorTupleIfOpaque().host());
  WriteParam(m, p.GetTupleOrPrecursorTupleIfOpaque().port());
  // Note: this is somewhat asymmetric with Read() to avoid extra copies during
  // serialization. The actual serialized wire format matches how std::optional
  // values are normally serialized: see `ParamTraits<std::optional<P>>`.
  const base::UnguessableToken* nonce = p.GetNonceForSerialization();
  WriteParam(m, nonce != nullptr);
  if (nonce) {
    WriteParam(m, *nonce);
  }
}

bool ParamTraits<url::Origin>::Read(const base::Pickle* m,
                                    base::PickleIterator* iter,
                                    url::Origin* p) {
  std::string scheme;
  std::string host;
  uint16_t port;
  std::optional<base::UnguessableToken> nonce_if_opaque;
  if (!ReadParam(m, iter, &scheme) || !ReadParam(m, iter, &host) ||
      !ReadParam(m, iter, &port) || !ReadParam(m, iter, &nonce_if_opaque)) {
    return false;
  }

  std::optional<url::Origin> creation_result =
      nonce_if_opaque
          ? url::Origin::UnsafelyCreateOpaqueOriginWithoutNormalization(
                scheme, host, port, url::Origin::Nonce(*nonce_if_opaque))
          : url::Origin::UnsafelyCreateTupleOriginWithoutNormalization(
                scheme, host, port);
  if (!creation_result)
    return false;

  *p = std::move(creation_result.value());
  return true;
}

void ParamTraits<url::Origin>::Log(const url::Origin& p, std::string* l) {
  l->append(p.Serialize());
}

void ParamTraits<net::SchemefulSite>::Write(base::Pickle* m,
                                            const net::SchemefulSite& p) {
  WriteParam(m, p.site_as_origin_);
}

bool ParamTraits<net::SchemefulSite>::Read(const base::Pickle* m,
                                           base::PickleIterator* iter,
                                           net::SchemefulSite* p) {
  url::Origin site_as_origin;
  if (!ReadParam(m, iter, &site_as_origin))
    return false;

  return net::SchemefulSite::FromWire(site_as_origin, p);
}

void ParamTraits<net::SchemefulSite>::Log(const net::SchemefulSite& p,
                                          std::string* l) {
  l->append(p.Serialize());
}

}  // namespace IPC

// Generation of IPC definitions.

// Generate constructors.
#undef SERVICES_NETWORK_PUBLIC_CPP_NET_IPC_PARAM_TRAITS_H_
#include "ipc/struct_constructor_macros.h"
#include "net_ipc_param_traits.h"

// Generate param traits write methods.
#undef SERVICES_NETWORK_PUBLIC_CPP_NET_IPC_PARAM_TRAITS_H_
#include "ipc/param_traits_write_macros.h"
namespace IPC {
#include "net_ipc_param_traits.h"
}  // namespace IPC

// Generate param traits read methods.
#undef SERVICES_NETWORK_PUBLIC_CPP_NET_IPC_PARAM_TRAITS_H_
#include "ipc/param_traits_read_macros.h"
namespace IPC {
#include "net_ipc_param_traits.h"
}  // namespace IPC

// Generate param traits log methods.
#undef SERVICES_NETWORK_PUBLIC_CPP_NET_IPC_PARAM_TRAITS_H_
#include "ipc/param_traits_log_macros.h"
namespace IPC {
#include "net_ipc_param_traits.h"
}  // namespace IPC
