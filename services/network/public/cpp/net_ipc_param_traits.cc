// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/net_ipc_param_traits.h"

#include "ipc/ipc_message_utils.h"
#include "ipc/ipc_mojo_param_traits.h"
#include "ipc/ipc_platform_file.h"
#include "net/http/http_util.h"

namespace IPC {

void ParamTraits<net::AuthChallengeInfo>::Write(base::Pickle* m,
                                                const param_type& p) {
  WriteParam(m, p.is_proxy);
  WriteParam(m, p.challenger);
  WriteParam(m, p.scheme);
  WriteParam(m, p.realm);
  WriteParam(m, p.challenge);
  WriteParam(m, p.path);
}

bool ParamTraits<net::AuthChallengeInfo>::Read(const base::Pickle* m,
                                               base::PickleIterator* iter,
                                               param_type* r) {
  return ReadParam(m, iter, &r->is_proxy) &&
         ReadParam(m, iter, &r->challenger) && ReadParam(m, iter, &r->scheme) &&
         ReadParam(m, iter, &r->realm) && ReadParam(m, iter, &r->challenge) &&
         ReadParam(m, iter, &r->path);
}

void ParamTraits<net::AuthChallengeInfo>::Log(const param_type& p,
                                              std::string* l) {
  l->append("<AuthChallengeInfo>");
}

void ParamTraits<net::AuthCredentials>::Write(base::Pickle* m,
                                              const param_type& p) {
  WriteParam(m, p.username());
  WriteParam(m, p.password());
}

bool ParamTraits<net::AuthCredentials>::Read(const base::Pickle* m,
                                             base::PickleIterator* iter,
                                             param_type* r) {
  base::string16 username;
  bool read_username = ReadParam(m, iter, &username);
  base::string16 password;
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
  WriteParam(m, p.has_md2);
  WriteParam(m, p.has_md4);
  WriteParam(m, p.has_md5);
  WriteParam(m, p.has_sha1);
  WriteParam(m, p.has_sha1_leaf);
  WriteParam(m, p.public_key_hashes);
  WriteParam(m, p.is_issued_by_known_root);
  WriteParam(m, p.is_issued_by_additional_trust_anchor);
  WriteParam(m, p.ocsp_result);
}

bool ParamTraits<net::CertVerifyResult>::Read(const base::Pickle* m,
                                              base::PickleIterator* iter,
                                              param_type* r) {
  return ReadParam(m, iter, &r->verified_cert) &&
         ReadParam(m, iter, &r->cert_status) &&
         ReadParam(m, iter, &r->has_md2) && ReadParam(m, iter, &r->has_md4) &&
         ReadParam(m, iter, &r->has_md5) && ReadParam(m, iter, &r->has_sha1) &&
         ReadParam(m, iter, &r->has_sha1_leaf) &&
         ReadParam(m, iter, &r->public_key_hashes) &&
         ReadParam(m, iter, &r->is_issued_by_known_root) &&
         ReadParam(m, iter, &r->is_issued_by_additional_trust_anchor) &&
         ReadParam(m, iter, &r->ocsp_result);
}

void ParamTraits<net::CertVerifyResult>::Log(const param_type& p,
                                             std::string* l) {
  l->append("<CertVerifyResult>");
}

void ParamTraits<net::ct::CTVerifyResult>::Write(base::Pickle* m,
                                                 const param_type& p) {
  WriteParam(m, p.scts);
  WriteParam(m, p.policy_compliance);
  WriteParam(m, p.policy_compliance_required);
}

bool ParamTraits<net::ct::CTVerifyResult>::Read(const base::Pickle* m,
                                                base::PickleIterator* iter,
                                                param_type* r) {
  return ReadParam(m, iter, &r->scts) &&
         ReadParam(m, iter, &r->policy_compliance) &&
         ReadParam(m, iter, &r->policy_compliance_required);
}

void ParamTraits<net::ct::CTVerifyResult>::Log(const param_type& p,
                                               std::string* l) {
  l->append("<CTVerifyResult>");
}

void ParamTraits<net::HashValue>::Write(base::Pickle* m, const param_type& p) {
  WriteParam(m, p.ToString());
}

bool ParamTraits<net::HashValue>::Read(const base::Pickle* m,
                                       base::PickleIterator* iter,
                                       param_type* r) {
  std::string str;
  return ReadParam(m, iter, &str) && r->FromString(str);
}

void ParamTraits<net::HashValue>::Log(const param_type& p, std::string* l) {
  l->append("<HashValue>");
}

void ParamTraits<net::HostPortPair>::Write(base::Pickle* m,
                                           const param_type& p) {
  WriteParam(m, p.host());
  WriteParam(m, p.port());
}

bool ParamTraits<net::HostPortPair>::Read(const base::Pickle* m,
                                          base::PickleIterator* iter,
                                          param_type* r) {
  std::string host;
  uint16_t port;
  if (!ReadParam(m, iter, &host) || !ReadParam(m, iter, &port))
    return false;

  r->set_host(host);
  r->set_port(port);
  return true;
}

void ParamTraits<net::HostPortPair>::Log(const param_type& p, std::string* l) {
  l->append(p.ToString());
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
  base::StackVector<uint8_t, 16> bytes;
  for (uint8_t byte : p.bytes())
    bytes->push_back(byte);
  WriteParam(m, bytes);
}

bool ParamTraits<net::IPAddress>::Read(const base::Pickle* m,
                                       base::PickleIterator* iter,
                                       param_type* p) {
  base::StackVector<uint8_t, 16> bytes;
  if (!ReadParam(m, iter, &bytes))
    return false;
  if (bytes->size() > 16)
    return false;
  *p = net::IPAddress(bytes->data(), bytes->size());
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
  int size;
  if (!iter->ReadLength(&size))
    return false;
  for (int i = 0; i < size; ++i) {
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

void ParamTraits<net::ProxyServer>::Write(base::Pickle* m,
                                          const param_type& p) {
  net::ProxyServer::Scheme scheme = p.scheme();
  WriteParam(m, scheme);
  // When scheme is either 'direct' or 'invalid' |host_port_pair|
  // should not be called, as per the method implementation body.
  if (scheme != net::ProxyServer::SCHEME_DIRECT &&
      scheme != net::ProxyServer::SCHEME_INVALID) {
    WriteParam(m, p.host_port_pair());
  }
  WriteParam(m, p.is_trusted_proxy());
}

bool ParamTraits<net::ProxyServer>::Read(const base::Pickle* m,
                                         base::PickleIterator* iter,
                                         param_type* r) {
  net::ProxyServer::Scheme scheme;
  bool is_trusted_proxy = false;
  if (!ReadParam(m, iter, &scheme))
    return false;

  // When scheme is either 'direct' or 'invalid' |host_port_pair|
  // should not be called, as per the method implementation body.
  net::HostPortPair host_port_pair;
  if (scheme != net::ProxyServer::SCHEME_DIRECT &&
      scheme != net::ProxyServer::SCHEME_INVALID &&
      !ReadParam(m, iter, &host_port_pair)) {
    return false;
  }

  if (!ReadParam(m, iter, &is_trusted_proxy))
    return false;

  *r = net::ProxyServer(scheme, host_port_pair, is_trusted_proxy);
  return true;
}

void ParamTraits<net::ProxyServer>::Log(const param_type& p, std::string* l) {
  l->append("<ProxyServer>");
}

void ParamTraits<net::OCSPVerifyResult>::Write(base::Pickle* m,
                                               const param_type& p) {
  WriteParam(m, p.response_status);
  WriteParam(m, p.revocation_status);
}

bool ParamTraits<net::OCSPVerifyResult>::Read(const base::Pickle* m,
                                              base::PickleIterator* iter,
                                              param_type* r) {
  return ReadParam(m, iter, &r->response_status) &&
         ReadParam(m, iter, &r->revocation_status);
}

void ParamTraits<net::OCSPVerifyResult>::Log(const param_type& p,
                                             std::string* l) {
  l->append("<OCSPVerifyResult>");
}

void ParamTraits<scoped_refptr<net::SSLCertRequestInfo>>::Write(
    base::Pickle* m,
    const param_type& p) {
  DCHECK(p);
  WriteParam(m, p->host_and_port);
  WriteParam(m, p->is_proxy);
  WriteParam(m, p->cert_authorities);
  WriteParam(m, p->cert_key_types);
}

bool ParamTraits<scoped_refptr<net::SSLCertRequestInfo>>::Read(
    const base::Pickle* m,
    base::PickleIterator* iter,
    param_type* r) {
  *r = base::MakeRefCounted<net::SSLCertRequestInfo>();
  return ReadParam(m, iter, &(*r)->host_and_port) &&
         ReadParam(m, iter, &(*r)->is_proxy) &&
         ReadParam(m, iter, &(*r)->cert_authorities) &&
         ReadParam(m, iter, &(*r)->cert_key_types);
}

void ParamTraits<scoped_refptr<net::SSLCertRequestInfo>>::Log(
    const param_type& p,
    std::string* l) {
  l->append("<SSLCertRequestInfo>");
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
  WriteParam(m, p.handshake_type);
  WriteParam(m, p.public_key_hashes);
  WriteParam(m, p.pinning_failure_log);
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
         ReadParam(m, iter, &r->handshake_type) &&
         ReadParam(m, iter, &r->public_key_hashes) &&
         ReadParam(m, iter, &r->pinning_failure_log) &&
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
  WriteParam(m, p.connect_timing.dns_start);
  WriteParam(m, p.connect_timing.dns_end);
  WriteParam(m, p.connect_timing.connect_start);
  WriteParam(m, p.connect_timing.connect_end);
  WriteParam(m, p.connect_timing.ssl_start);
  WriteParam(m, p.connect_timing.ssl_end);
  WriteParam(m, p.send_start);
  WriteParam(m, p.send_end);
  WriteParam(m, p.receive_headers_start);
  WriteParam(m, p.receive_headers_end);
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
         ReadParam(m, iter, &r->connect_timing.dns_start) &&
         ReadParam(m, iter, &r->connect_timing.dns_end) &&
         ReadParam(m, iter, &r->connect_timing.connect_start) &&
         ReadParam(m, iter, &r->connect_timing.connect_end) &&
         ReadParam(m, iter, &r->connect_timing.ssl_start) &&
         ReadParam(m, iter, &r->connect_timing.ssl_end) &&
         ReadParam(m, iter, &r->send_start) &&
         ReadParam(m, iter, &r->send_end) &&
         ReadParam(m, iter, &r->receive_headers_start) &&
         ReadParam(m, iter, &r->receive_headers_end) &&
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
  LogParam(p.connect_timing.dns_start, l);
  l->append(", ");
  LogParam(p.connect_timing.dns_end, l);
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
  LogParam(p.push_start, l);
  l->append(", ");
  LogParam(p.push_end, l);
  l->append(")");
}

void ParamTraits<url::Origin>::Write(base::Pickle* m, const url::Origin& p) {
  WriteParam(m, p.GetTupleOrPrecursorTupleIfOpaque().scheme());
  WriteParam(m, p.GetTupleOrPrecursorTupleIfOpaque().host());
  WriteParam(m, p.GetTupleOrPrecursorTupleIfOpaque().port());
  WriteParam(m, p.GetNonceForSerialization());
}

bool ParamTraits<url::Origin>::Read(const base::Pickle* m,
                                    base::PickleIterator* iter,
                                    url::Origin* p) {
  std::string scheme;
  std::string host;
  uint16_t port;
  base::Optional<base::UnguessableToken> nonce_if_opaque;
  if (!ReadParam(m, iter, &scheme) || !ReadParam(m, iter, &host) ||
      !ReadParam(m, iter, &port) || !ReadParam(m, iter, &nonce_if_opaque)) {
    return false;
  }

  base::Optional<url::Origin> creation_result =
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
