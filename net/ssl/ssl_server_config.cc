// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/ssl_server_config.h"

#include "net/socket/ssl_client_socket.h"
#include "net/ssl/ssl_config.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"

namespace net {

SSLServerCredential::SSLServerCredential() = default;
SSLServerCredential::SSLServerCredential(SSLServerCredential&& other) = default;
SSLServerCredential& SSLServerCredential::operator=(
    SSLServerCredential&& other) = default;
SSLServerCredential::~SSLServerCredential() = default;

SSLServerConfig::SSLServerConfig() = default;

SSLServerConfig::SSLServerConfig(const SSLServerConfig& other) = default;

SSLServerConfig::~SSLServerConfig() = default;

}  // namespace net
