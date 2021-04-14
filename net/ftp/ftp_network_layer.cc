// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ftp/ftp_network_layer.h"

#include "base/check.h"
#include "net/ftp/ftp_network_session.h"
#include "net/ftp/ftp_network_transaction.h"
#include "net/socket/client_socket_factory.h"

namespace net {

FtpNetworkLayer::FtpNetworkLayer(HostResolver* host_resolver)
    : session_(new FtpNetworkSession(host_resolver)),
      suspended_(false) {
  DCHECK(host_resolver);
}

FtpNetworkLayer::~FtpNetworkLayer() = default;

std::unique_ptr<FtpTransaction> FtpNetworkLayer::CreateTransaction() {
  if (suspended_)
    return nullptr;

  return std::make_unique<FtpNetworkTransaction>(
      session_->host_resolver(), ClientSocketFactory::GetDefaultFactory());
}

void FtpNetworkLayer::Suspend(bool suspend) {
  suspended_ = suspend;
}

}  // namespace net
