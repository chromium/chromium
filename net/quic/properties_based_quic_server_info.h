// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_PROPERTIES_BASED_QUIC_SERVER_INFO_H_
#define NET_QUIC_PROPERTIES_BASED_QUIC_SERVER_INFO_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "net/base/net_export.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/privacy_mode.h"
#include "net/quic/quic_server_info.h"

namespace net {

class HttpServerProperties;

// PropertiesBasedQuicServerInfo fetches information about a QUIC server from
// HttpServerProperties. Since the information is defined to be non-sensitive,
// it's ok for us to keep it on disk.
class NET_EXPORT_PRIVATE PropertiesBasedQuicServerInfo : public QuicServerInfo {
 public:
  PropertiesBasedQuicServerInfo(
      const quic::QuicServerId& server_id,
      PrivacyMode privacy_mode,
      const NetworkAnonymizationKey& network_anonymization_key,
      HttpServerProperties* http_server_properties);

  PropertiesBasedQuicServerInfo(const PropertiesBasedQuicServerInfo&) = delete;
  PropertiesBasedQuicServerInfo& operator=(
      const PropertiesBasedQuicServerInfo&) = delete;

  ~PropertiesBasedQuicServerInfo() override;

  // QuicServerInfo implementation.
  bool Load() override;
  void Persist() override;

 private:
  const PrivacyMode privacy_mode_ = PRIVACY_MODE_DISABLED;
  const NetworkAnonymizationKey network_anonymization_key_;
  const raw_ptr<HttpServerProperties> http_server_properties_;
};

}  // namespace net

#endif  // NET_QUIC_PROPERTIES_BASED_QUIC_SERVER_INFO_H_
