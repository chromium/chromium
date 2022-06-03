// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/socket.h"

#include "net/base/net_errors.h"

namespace net {

Socket::Socket() = default;

Socket::~Socket() = default;

int Socket::ReadIfReady(IOBuffer* buf,
                        int buf_len,
                        CompletionOnceCallback callback) {
  return ERR_READ_IF_READY_NOT_IMPLEMENTED;
}

int Socket::CancelReadIfReady() {
  return ERR_READ_IF_READY_NOT_IMPLEMENTED;
}

void Socket::SetDnsAliases(std::vector<std::string> aliases) {
  if (aliases == std::vector<std::string>({""})) {
    // Reset field to empty vector. Necessary because some tests and other
    // inputs still use a trivial canonical name of std::string().
    dns_aliases_ = std::vector<std::string>();
    return;
  }

  dns_aliases_ = std::move(aliases);
}

const std::vector<std::string>& Socket::GetDnsAliases() const {
  return dns_aliases_;
}

}  // namespace net
