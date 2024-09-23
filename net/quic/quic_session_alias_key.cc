// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_session_alias_key.h"

#include <tuple>

#include "net/quic/quic_session_key.h"
#include "url/scheme_host_port.h"

namespace net {

QuicSessionAliasKey::QuicSessionAliasKey(url::SchemeHostPort destination,
                                         QuicSessionKey session_key)
    : destination_(std::move(destination)),
      session_key_(std::move(session_key)) {}

bool QuicSessionAliasKey::operator<(const QuicSessionAliasKey& other) const {
  return std::tie(destination_, session_key_) <
         std::tie(other.destination_, other.session_key_);
}

bool QuicSessionAliasKey::operator==(const QuicSessionAliasKey& other) const {
  return destination_ == other.destination_ &&
         session_key_ == other.session_key_;
}

}  // namespace net
