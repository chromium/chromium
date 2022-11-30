// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/ssl_server_config.h"

#include "net/socket/ssl_client_socket.h"
#include "net/ssl/ssl_config.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"

namespace net {

SSLServerConfig::SSLServerConfig() = default;

SSLServerConfig::SSLServerConfig(const SSLServerConfig& other) = default;

SSLServerConfig::~SSLServerConfig() = default;

SSLServerConfig::ECHKeysContainer::ECHKeysContainer() = default;

SSLServerConfig::ECHKeysContainer::ECHKeysContainer(
    bssl::UniquePtr<SSL_ECH_KEYS> keys)
    : keys_(std::move(keys)) {}

SSLServerConfig::ECHKeysContainer::~ECHKeysContainer() = default;

SSLServerConfig::ECHKeysContainer::ECHKeysContainer(
    const SSLServerConfig::ECHKeysContainer& other)
    : keys_(bssl::UpRef(other.keys_)) {}

SSLServerConfig::ECHKeysContainer& SSLServerConfig::ECHKeysContainer::operator=(
    const SSLServerConfig::ECHKeysContainer& other) {
  keys_ = bssl::UpRef(other.keys_);
  return *this;
}

void SSLServerConfig::ECHKeysContainer::reset(SSL_ECH_KEYS* keys) {
  keys_.reset(keys);
}

}  // namespace net
