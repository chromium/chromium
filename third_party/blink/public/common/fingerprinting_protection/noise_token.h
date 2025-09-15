// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_FINGERPRINTING_PROTECTION_NOISE_TOKEN_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_FINGERPRINTING_PROTECTION_NOISE_TOKEN_H_

#include <compare>
#include <cstdint>

#include "mojo/public/cpp/bindings/default_construct_tag.h"
#include "third_party/blink/public/common/common_export.h"

namespace blink {

// A wrapper class for a uint64_t noise token.
class BLINK_COMMON_EXPORT NoiseToken final {
 public:
  explicit NoiseToken(uint64_t token) : token_(token) {}

  // This constructor is only used by Mojo for typemap conversions.
  explicit NoiseToken(mojo::DefaultConstruct::Tag) {}

  uint64_t Value() const { return token_; }

  auto operator<=>(const NoiseToken& other) const = default;

 private:
  // Non-const because Mojo uses a constructor that doesn't initialize it for
  // type map conversions.
  uint64_t token_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_FINGERPRINTING_PROTECTION_NOISE_TOKEN_H_
