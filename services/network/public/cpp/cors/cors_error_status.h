// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_CORS_CORS_ERROR_STATUS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_CORS_CORS_ERROR_STATUS_H_

#include <iosfwd>
#include <string>

#include "base/component_export.h"
#include "base/unguessable_token.h"
#include "mojo/public/cpp/bindings/default_construct_tag.h"
#include "services/network/public/mojom/cors.mojom-shared.h"
#include "services/network/public/mojom/ip_address_space.mojom-shared.h"

namespace network {

// Type-mapped to `network::mojom::CorsErrorStatus`.
struct COMPONENT_EXPORT(NETWORK_CPP_BASE) CorsErrorStatus {
  // Instances of this type are copyable and efficiently movable.
  CorsErrorStatus(const CorsErrorStatus&);
  CorsErrorStatus& operator=(const CorsErrorStatus&);
  CorsErrorStatus(CorsErrorStatus&&);
  CorsErrorStatus& operator=(CorsErrorStatus&&);

  explicit CorsErrorStatus(mojom::CorsError cors_error);
  CorsErrorStatus(mojom::CorsError cors_error,
                  const std::string& failed_parameter);

  // Constructor for Private Network Access errors.
  CorsErrorStatus(mojom::CorsError cors_error,
                  mojom::IPAddressSpace target_address_space,
                  mojom::IPAddressSpace resource_address_space);

  ~CorsErrorStatus();

  bool operator==(const CorsErrorStatus& rhs) const;
  bool operator!=(const CorsErrorStatus& rhs) const { return !(*this == rhs); }

  // This constructor is used by generated IPC serialization code.
  explicit CorsErrorStatus(mojo::DefaultConstruct::Tag);

  // NOTE: This value is meaningless and should be overridden immediately either
  // by a constructor or by Mojo deserialization code.
  mojom::CorsError cors_error = mojom::CorsError::kMaxValue;

  std::string failed_parameter;
  mojom::IPAddressSpace target_address_space = mojom::IPAddressSpace::kUnknown;
  mojom::IPAddressSpace resource_address_space =
      mojom::IPAddressSpace::kUnknown;
  bool has_authorization_covered_by_wildcard_on_preflight = false;
  base::UnguessableToken issue_id = base::UnguessableToken::Create();
};

// CorsErrorStatus instances are streamable for ease of debugging.
COMPONENT_EXPORT(NETWORK_CPP_BASE)
std::ostream& operator<<(std::ostream& os, const CorsErrorStatus& status);

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_CORS_CORS_ERROR_STATUS_H_
