// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_CORS_CORS_ERROR_STATUS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_CORS_CORS_ERROR_STATUS_H_

#include <string>

#include "base/component_export.h"
#include "base/memory/scoped_refptr.h"
#include "net/http/http_response_headers.h"

namespace network {

namespace mojom {
enum class CorsError : int32_t;
}

struct COMPONENT_EXPORT(NETWORK_CPP_BASE) CorsErrorStatus {
  // This constructor is used by generated IPC serialization code.
  // Should not use this explicitly.
  // TODO(toyoshim, yhirano): Exploring a way to make this private, and allows
  // only serialization code for mojo can access.
  CorsErrorStatus();

  CorsErrorStatus(const CorsErrorStatus& status);

  explicit CorsErrorStatus(mojom::CorsError error);
  CorsErrorStatus(mojom::CorsError error, const std::string& failed_parameter);

  ~CorsErrorStatus();

  bool operator==(const CorsErrorStatus& rhs) const;
  bool operator!=(const CorsErrorStatus& rhs) const { return !(*this == rhs); }

  mojom::CorsError cors_error;

  // Contains request method name, or header name that didn't pass a CORS check.
  std::string failed_parameter;
};

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_CORS_CORS_ERROR_STATUS_H_
