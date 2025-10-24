// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/connection_allowlist.h"

namespace network {

ConnectionAllowlist::ConnectionAllowlist() = default;
ConnectionAllowlist::~ConnectionAllowlist() = default;

ConnectionAllowlist::ConnectionAllowlist(ConnectionAllowlist&&) = default;
ConnectionAllowlist& ConnectionAllowlist::operator=(ConnectionAllowlist&&) =
    default;

ConnectionAllowlist::ConnectionAllowlist(const ConnectionAllowlist&) = default;
ConnectionAllowlist& ConnectionAllowlist::operator=(
    const ConnectionAllowlist&) = default;

bool ConnectionAllowlist::operator==(const ConnectionAllowlist& other) const =
    default;

ConnectionAllowlists::ConnectionAllowlists() = default;
ConnectionAllowlists::~ConnectionAllowlists() = default;

ConnectionAllowlists::ConnectionAllowlists(ConnectionAllowlists&&) = default;
ConnectionAllowlists& ConnectionAllowlists::operator=(ConnectionAllowlists&&) =
    default;

ConnectionAllowlists::ConnectionAllowlists(const ConnectionAllowlists&) =
    default;
ConnectionAllowlists& ConnectionAllowlists::operator=(
    const ConnectionAllowlists&) = default;

bool ConnectionAllowlists::operator==(const ConnectionAllowlists& other) const =
    default;

}  // namespace network
