// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_LOAD_TIMING_INFO_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_LOAD_TIMING_INFO_MOJOM_TRAITS_H_

#include "mojo/public/cpp/bindings/struct_traits.h"
#include "net/base/load_timing_info.h"
#include "services/network/public/mojom/load_timing_info.mojom.h"

namespace mojo {

template <>
struct StructTraits<network::mojom::LoadTimingInfoConnectTimingDataView,
                    net::LoadTimingInfo::ConnectTiming> {
  static base::TimeTicks dns_start(
      const net::LoadTimingInfo::ConnectTiming& obj) {
    return obj.dns_start;
  }

  static base::TimeTicks dns_end(
      const net::LoadTimingInfo::ConnectTiming& obj) {
    return obj.dns_end;
  }

  static base::TimeTicks connect_start(
      const net::LoadTimingInfo::ConnectTiming& obj) {
    return obj.connect_start;
  }

  static base::TimeTicks connect_end(
      const net::LoadTimingInfo::ConnectTiming& obj) {
    return obj.connect_end;
  }

  static base::TimeTicks ssl_start(
      const net::LoadTimingInfo::ConnectTiming& obj) {
    return obj.ssl_start;
  }

  static base::TimeTicks ssl_end(
      const net::LoadTimingInfo::ConnectTiming& obj) {
    return obj.ssl_end;
  }

  static bool Read(network::mojom::LoadTimingInfoConnectTimingDataView obj,
                   net::LoadTimingInfo::ConnectTiming* output);
};

template <>
struct StructTraits<network::mojom::LoadTimingInfoDataView,
                    net::LoadTimingInfo> {
  static bool socket_reused(const net::LoadTimingInfo& obj) {
    return obj.socket_reused;
  }

  static uint32_t socket_log_id(const net::LoadTimingInfo& obj) {
    return obj.socket_log_id;
  }

  static base::Time request_start_time(const net::LoadTimingInfo& obj) {
    return obj.request_start_time;
  }

  static base::TimeTicks request_start(const net::LoadTimingInfo& obj) {
    return obj.request_start;
  }

  static base::TimeTicks proxy_resolve_start(const net::LoadTimingInfo& obj) {
    return obj.proxy_resolve_start;
  }

  static base::TimeTicks proxy_resolve_end(const net::LoadTimingInfo& obj) {
    return obj.proxy_resolve_end;
  }

  static net::LoadTimingInfo::ConnectTiming connect_timing(
      const net::LoadTimingInfo& obj) {
    return obj.connect_timing;
  }

  static base::TimeTicks send_start(const net::LoadTimingInfo& obj) {
    return obj.send_start;
  }

  static base::TimeTicks send_end(const net::LoadTimingInfo& obj) {
    return obj.send_end;
  }

  static base::TimeTicks receive_headers_start(const net::LoadTimingInfo& obj) {
    return obj.receive_headers_start;
  }

  static base::TimeTicks receive_headers_end(const net::LoadTimingInfo& obj) {
    return obj.receive_headers_end;
  }

  static base::TimeTicks push_start(const net::LoadTimingInfo& obj) {
    return obj.push_start;
  }

  static base::TimeTicks push_end(const net::LoadTimingInfo& obj) {
    return obj.push_end;
  }

  static bool Read(network::mojom::LoadTimingInfoDataView obj,
                   net::LoadTimingInfo* output);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_LOAD_TIMING_INFO_MOJOM_TRAITS_H_
