// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_P2P_PARAM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_P2P_PARAM_TRAITS_H_

// IPC messages for the P2P Transport API.

#include <stdint.h>

#include "base/component_export.h"
#include "base/time/time.h"
#include "ipc/ipc_message_macros.h"
#include "net/base/ip_address.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_interfaces.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/p2p_socket_type.h"
#include "third_party/webrtc/rtc_base/async_packet_socket.h"

#ifndef INTERNAL_SERVICES_NETWORK_PUBLIC_CPP_P2P_PARAM_TRAITS_H_
#define INTERNAL_SERVICES_NETWORK_PUBLIC_CPP_P2P_PARAM_TRAITS_H_

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT COMPONENT_EXPORT(NETWORK_CPP_BASE)

#endif  // INTERNAL_SERVICES_NETWORK_PUBLIC_CPP_P2P_PARAM_TRAITS_H_

IPC_ENUM_TRAITS_MAX_VALUE(network::P2PSocketType, network::P2P_SOCKET_TYPE_LAST)
IPC_ENUM_TRAITS_MAX_VALUE(network::P2PSocketOption,
                          network::P2P_SOCKET_OPT_MAX - 1)
IPC_ENUM_TRAITS_MAX_VALUE(net::NetworkChangeNotifier::ConnectionType,
                          net::NetworkChangeNotifier::CONNECTION_LAST)
IPC_ENUM_TRAITS_MIN_MAX_VALUE(rtc::DiffServCodePoint,
                              rtc::DSCP_NO_CHANGE,
                              rtc::DSCP_CS7)

IPC_STRUCT_TRAITS_BEGIN(rtc::PacketTimeUpdateParams)
  IPC_STRUCT_TRAITS_MEMBER(rtp_sendtime_extension_id)
  IPC_STRUCT_TRAITS_MEMBER(srtp_auth_key)
  IPC_STRUCT_TRAITS_MEMBER(srtp_auth_tag_len)
  IPC_STRUCT_TRAITS_MEMBER(srtp_packet_index)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(rtc::PacketOptions)
  IPC_STRUCT_TRAITS_MEMBER(dscp)
  IPC_STRUCT_TRAITS_MEMBER(packet_id)
  IPC_STRUCT_TRAITS_MEMBER(packet_time_params)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(network::P2PHostAndIPEndPoint)
  IPC_STRUCT_TRAITS_MEMBER(hostname)
  IPC_STRUCT_TRAITS_MEMBER(ip_address)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(network::P2PSendPacketMetrics)
  IPC_STRUCT_TRAITS_MEMBER(packet_id)
  IPC_STRUCT_TRAITS_MEMBER(rtc_packet_id)
  IPC_STRUCT_TRAITS_MEMBER(send_time_ms)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(network::P2PPortRange)
  IPC_STRUCT_TRAITS_MEMBER(min_port)
  IPC_STRUCT_TRAITS_MEMBER(max_port)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(network::P2PPacketInfo)
  IPC_STRUCT_TRAITS_MEMBER(destination)
  IPC_STRUCT_TRAITS_MEMBER(packet_options)
  IPC_STRUCT_TRAITS_MEMBER(packet_id)
IPC_STRUCT_TRAITS_END()

#endif  // SERVICES_NETWORK_PUBLIC_CPP_P2P_PARAM_TRAITS_H_
