// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_CONNECTION_ALLOWLIST_H_
#define SERVICES_NETWORK_PUBLIC_CPP_CONNECTION_ALLOWLIST_H_

#include <optional>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "services/network/public/mojom/connection_allowlist.mojom-shared.h"

namespace network {

// This implements a data structure holding information from a parsed
// `Connection-Allowlist` or `Connection-Allowlist-Report-Only` header.
//
// This struct is needed as we can't use the mojom generated struct directly
// from the blink public API, given that we cannot include .mojo.h there due to
// DEPS rules.
struct COMPONENT_EXPORT(NETWORK_CPP_CONNECTION_ALLOWLIST) ConnectionAllowlist {
  ConnectionAllowlist();
  ~ConnectionAllowlist();

  ConnectionAllowlist(ConnectionAllowlist&&);
  ConnectionAllowlist& operator=(ConnectionAllowlist&&);

  ConnectionAllowlist(const ConnectionAllowlist&);
  ConnectionAllowlist& operator=(const ConnectionAllowlist&);

  bool operator==(const ConnectionAllowlist&) const;

  std::vector<std::string> allowlist;
  std::optional<std::string> reporting_endpoint;
  std::vector<mojom::ConnectionAllowlistIssue> issues;
};

// The set of allowlists associated with a given response, typemapped for the
// same reason.
struct COMPONENT_EXPORT(NETWORK_CPP_CONNECTION_ALLOWLIST) ConnectionAllowlists {
  ConnectionAllowlists();
  ~ConnectionAllowlists();

  ConnectionAllowlists(ConnectionAllowlists&&);
  ConnectionAllowlists& operator=(ConnectionAllowlists&&);

  ConnectionAllowlists(const ConnectionAllowlists&);
  ConnectionAllowlists& operator=(const ConnectionAllowlists&);

  bool operator==(const ConnectionAllowlists&) const;

  std::optional<ConnectionAllowlist> enforced;
  std::optional<ConnectionAllowlist> report_only;
};

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_CONNECTION_ALLOWLIST_H_
