// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_INSTANCE_IDENTITY_TOKEN_H_
#define REMOTING_BASE_INSTANCE_IDENTITY_TOKEN_H_

#include <optional>
#include <ostream>
#include <string>
#include <string_view>

#include "base/values.h"

namespace remoting {

// Represents a decoded JWT retrieved from the Compute Engine metadata server.
class InstanceIdentityToken final {
 public:
  // Returns a new instance if |jwt| is well-formed, otherwise nullopt. Note
  // that |jwt| could be expired and the Create() function will still succeed as
  // long as all of the required fields are present.
  static std::optional<InstanceIdentityToken> Create(std::string_view jwt);

  InstanceIdentityToken(InstanceIdentityToken&& other) = default;
  InstanceIdentityToken& operator=(InstanceIdentityToken&& other) = default;
  InstanceIdentityToken(const InstanceIdentityToken&) = delete;
  InstanceIdentityToken& operator=(const InstanceIdentityToken&) = delete;

  ~InstanceIdentityToken();

  const base::Value::Dict& header() const { return header_; }
  const base::Value::Dict& payload() const { return payload_; }

 private:
  InstanceIdentityToken(base::Value::Dict header, base::Value::Dict payload);

  base::Value::Dict header_;
  base::Value::Dict payload_;
};

std::ostream& operator<<(std::ostream& out, const InstanceIdentityToken& token);

}  // namespace remoting

#endif  // REMOTING_BASE_INSTANCE_IDENTITY_TOKEN_H_
