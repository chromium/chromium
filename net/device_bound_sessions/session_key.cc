// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/session_key.h"

namespace net::device_bound_sessions {

SessionKey::SessionKey() = default;
SessionKey::SessionKey(SchemefulSite site, Id id) : site(site), id(id) {}
SessionKey::~SessionKey() = default;

SessionKey::SessionKey(const SessionKey&) = default;
SessionKey& SessionKey::operator=(const SessionKey&) = default;

SessionKey::SessionKey(SessionKey&&) = default;
SessionKey& SessionKey::operator=(SessionKey&&) = default;

bool SessionKey::operator==(const SessionKey& other) const = default;
bool SessionKey::operator<(const SessionKey& other) const {
  if (site != other.site) {
    return site < other.site;
  }
  return id.value() < other.id.value();
}

}  // namespace net::device_bound_sessions
