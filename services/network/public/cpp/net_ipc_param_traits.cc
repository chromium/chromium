// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/net_ipc_param_traits.h"

#include <string_view>

#include "ipc/mojo_param_traits.h"
#include "ipc/param_traits_utils.h"
#include "net/base/hash_value.h"
#include "net/http/http_util.h"

namespace IPC {

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

}  // namespace IPC

// Generation of IPC definitions.

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
