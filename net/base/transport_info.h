// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_TRANSPORT_INFO_H_
#define NET_BASE_TRANSPORT_INFO_H_

#include <iosfwd>
#include <string>

#include "base/strings/string_piece.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_export.h"

namespace net {

// Specifies the type of a network transport over which a resource is loaded.
enum class TransportType {
  // The transport was established directly to a peer.
  kDirect,
  // The transport was established to a proxy of some kind.
  kProxied,
  // The transport was "established" to a cache entry.
  kCached,
  // Same as `kCached`, but the resource was initially loaded through a proxy.
  kCachedFromProxy,
};

// Returns a string representation of the given transport type.
// The returned StringPiece is static, has no lifetime restrictions.
NET_EXPORT base::StringPiece TransportTypeToString(TransportType type);

// Describes a network transport.
struct NET_EXPORT TransportInfo {
  TransportInfo();
  TransportInfo(TransportType type_arg,
                IPEndPoint endpoint_arg,
                std::string accept_ch_frame_arg);
  TransportInfo(const TransportInfo&);
  ~TransportInfo();

  // Instances of this type are comparable for equality.
  bool operator==(const TransportInfo& other) const;
  bool operator!=(const TransportInfo& other) const;

  // Returns a string representation of this struct, suitable for debugging.
  std::string ToString() const;

  // The type of the transport.
  TransportType type = TransportType::kDirect;

  // If `type` is `kDirect`, then this identifies the peer endpoint.
  // If `type` is `kProxied`, then this identifies the proxy endpoint.
  // If `type` is `kCached`, then this identifies the peer endpoint from which
  // the resource was originally loaded.
  // If `type` is `kCachedFromProxy`, then this identifies the proxy endpoint
  // from which the resource was originally loaded.
  IPEndPoint endpoint;

  // The value of the ACCEPT_CH HTTP2/3 frame, as pulled in through ALPS.
  //
  // Invariant: if `type` is `kCached` or `kCachedFromProxy`, then this is
  // empty.
  std::string accept_ch_frame;
};

// Instances of these types are streamable for easier debugging.
NET_EXPORT std::ostream& operator<<(std::ostream& out, TransportType type);
NET_EXPORT std::ostream& operator<<(std::ostream& out,
                                    const TransportInfo& info);

}  // namespace net

#endif  // NET_BASE_TRANSPORT_INFO_H_
