// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cors/cors_error_status.h"

#include "net/base/net_errors.h"
#include "services/network/public/mojom/cors.mojom-shared.h"

namespace network {

// Note: |cors_error| is initialized to kLast to keep the value inside the
// valid enum value range. The value is meaningless and should be overriden
// immediately by IPC desrtialization code.
CorsErrorStatus::CorsErrorStatus()
    : CorsErrorStatus(mojom::CorsError::kMaxValue) {}

CorsErrorStatus::CorsErrorStatus(const CorsErrorStatus& status) = default;

CorsErrorStatus::CorsErrorStatus(mojom::CorsError error) : cors_error(error) {}

CorsErrorStatus::CorsErrorStatus(mojom::CorsError error,
                                 const std::string& failed_parameter)
    : cors_error(error), failed_parameter(failed_parameter) {}

CorsErrorStatus::~CorsErrorStatus() = default;

bool CorsErrorStatus::operator==(const CorsErrorStatus& rhs) const {
  return cors_error == rhs.cors_error &&
         failed_parameter == rhs.failed_parameter;
}

}  // namespace network
