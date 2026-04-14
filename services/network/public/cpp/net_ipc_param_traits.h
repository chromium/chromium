// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_NET_IPC_PARAM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_NET_IPC_PARAM_TRAITS_H_

#include "base/component_export.h"
#include "base/memory/scoped_refptr.h"
#include "base/pickle.h"
#include "ipc/param_traits.h"
#include "ipc/param_traits_macros.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/load_timing_info.h"
#include "net/base/proxy_server.h"
#include "net/base/request_priority.h"
#include "net/base/schemeful_site.h"
#include "net/cert/signed_certificate_timestamp.h"
#include "net/cert/signed_certificate_timestamp_and_status.h"
#include "net/dns/public/resolve_error_info.h"
#include "net/http/http_connection_info.h"
#include "net/http/http_request_headers.h"
#include "net/nqe/effective_connection_type.h"
#include "net/ssl/ssl_info.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/referrer_policy.h"
#include "third_party/boringssl/src/include/openssl/pki/ocsp.h"
#include "url/ipc/url_param_traits.h"
#include "url/origin.h"

// This file defines IPC::ParamTraits for net:: classes / structs.
// For IPC::ParamTraits for network:: class / structs,
// see network_ipc_param_traits.h.

#ifndef INTERNAL_SERVICES_NETWORK_PUBLIC_CPP_NET_IPC_PARAM_TRAITS_H_
#define INTERNAL_SERVICES_NETWORK_PUBLIC_CPP_NET_IPC_PARAM_TRAITS_H_

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT COMPONENT_EXPORT(NETWORK_CPP_NETWORK_PARAM)

namespace IPC {

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_NETWORK_PARAM)
    ParamTraits<net::IPEndPoint> {
  typedef net::IPEndPoint param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* p);
};

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_NETWORK_PARAM) ParamTraits<net::IPAddress> {
  typedef net::IPAddress param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* p);
};

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_NETWORK_PARAM)
    ParamTraits<net::HttpRequestHeaders> {
  typedef net::HttpRequestHeaders param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
};

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_NETWORK_PARAM)
    ParamTraits<net::ResolveErrorInfo> {
  typedef net::ResolveErrorInfo param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
};

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_NETWORK_PARAM)
    ParamTraits<net::LoadTimingInfo> {
  typedef net::LoadTimingInfo param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
};

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_NETWORK_PARAM)
    ParamTraits<net::SiteForCookies> {
  typedef net::SiteForCookies param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
};

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_NETWORK_PARAM) ParamTraits<url::Origin> {
  typedef url::Origin param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* p);
};

template <>
struct COMPONENT_EXPORT(NETWORK_CPP_NETWORK_PARAM)
    ParamTraits<net::SchemefulSite> {
  typedef net::SchemefulSite param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* p);
};

}  // namespace IPC

#endif  // INTERNAL_SERVICES_NETWORK_PUBLIC_CPP_NET_IPC_PARAM_TRAITS_H_

IPC_ENUM_TRAITS(net::ProxyServer::Scheme)  // BitMask.

IPC_ENUM_TRAITS_MAX_VALUE(net::RequestPriority, net::MAXIMUM_PRIORITY)

IPC_ENUM_TRAITS_MAX_VALUE(net::ReferrerPolicy, net::ReferrerPolicy::MAX)

IPC_STRUCT_TRAITS_BEGIN(net::HttpRequestHeaders::HeaderKeyValuePair)
  IPC_STRUCT_TRAITS_MEMBER(key)
  IPC_STRUCT_TRAITS_MEMBER(value)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(net::MutableNetworkTrafficAnnotationTag)
  IPC_STRUCT_TRAITS_MEMBER(unique_id_hash_code)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(net::RedirectInfo)
  IPC_STRUCT_TRAITS_MEMBER(original_initiator)
  IPC_STRUCT_TRAITS_MEMBER(status_code)
  IPC_STRUCT_TRAITS_MEMBER(new_method)
  IPC_STRUCT_TRAITS_MEMBER(new_url)
  IPC_STRUCT_TRAITS_MEMBER(new_site_for_cookies)
  IPC_STRUCT_TRAITS_MEMBER(new_referrer)
  IPC_STRUCT_TRAITS_MEMBER(insecure_scheme_was_upgraded)
  IPC_STRUCT_TRAITS_MEMBER(is_signed_exchange_fallback_redirect)
  IPC_STRUCT_TRAITS_MEMBER(new_referrer_policy)
  IPC_STRUCT_TRAITS_MEMBER(critical_ch_restart_time)
IPC_STRUCT_TRAITS_END()

IPC_ENUM_TRAITS_MAX_VALUE(net::HttpConnectionInfo,
                          net::HttpConnectionInfo::kMaxValue)

IPC_ENUM_TRAITS_MAX_VALUE(net::EffectiveConnectionType,
                          net::EFFECTIVE_CONNECTION_TYPE_LAST - 1)

#endif  // SERVICES_NETWORK_PUBLIC_CPP_NET_IPC_PARAM_TRAITS_H_
