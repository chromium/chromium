// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_CORS_CORS_ERROR_STATUS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_CORS_CORS_ERROR_STATUS_H_

#include <iosfwd>
#include <string>

#include "base/component_export.h"
#include "base/memory/scoped_refptr.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/mojom/cors.mojom-shared.h"
#include "services/network/public/mojom/ip_address_space.mojom-shared.h"

namespace network {

// WARNING: When adding fields to this truct, do not forget to add them in
// services/network/public/cpp/network_ipc_param_traits.h too.
struct COMPONENT_EXPORT(NETWORK_CPP_BASE) CorsErrorStatus {
  // This constructor is used by generated IPC serialization code.
  // Should not use this explicitly.
  // TODO(toyoshim, yhirano): Exploring a way to make this private, and allows
  // only serialization code for mojo can access.
  CorsErrorStatus();

  // Instances of this type are copyable and efficiently movable.
  CorsErrorStatus(const CorsErrorStatus&);
  CorsErrorStatus& operator=(const CorsErrorStatus&);
  CorsErrorStatus(CorsErrorStatus&&);
  CorsErrorStatus& operator=(CorsErrorStatus&&);

  explicit CorsErrorStatus(mojom::CorsError cors_error);
  CorsErrorStatus(mojom::CorsError cors_error,
                  const std::string& failed_parameter);

  // Constructor for CORS-RFC1918 errors.
  // Sets `cors_error` to `kInsecurePrivateNetwork`.
  explicit CorsErrorStatus(mojom::IPAddressSpace resource_address_space);

  ~CorsErrorStatus();

  bool operator==(const CorsErrorStatus& rhs) const;
  bool operator!=(const CorsErrorStatus& rhs) const { return !(*this == rhs); }

  // NOTE: This value is meaningless and should be overridden immediately either
  // by a constructor or by IPC deserialization code.
  mojom::CorsError cors_error = mojom::CorsError::kMaxValue;

  // Contains request method name, or header name that didn't pass a CORS check.
  std::string failed_parameter;

  // The address space of the requested resource.
  //
  // Only set if `cors_error == kInsecurePrivateNetwork`.
  mojom::IPAddressSpace resource_address_space =
      mojom::IPAddressSpace::kUnknown;
};

// CorsErrorStatus instances are streamable for ease of debugging.
COMPONENT_EXPORT(NETWORK_CPP_BASE)
std::ostream& operator<<(std::ostream& os, const CorsErrorStatus& status);

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_CORS_CORS_ERROR_STATUS_H_
