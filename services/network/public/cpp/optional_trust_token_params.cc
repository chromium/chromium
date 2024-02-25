// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/optional_trust_token_params.h"

namespace network {

OptionalTrustTokenParams::OptionalTrustTokenParams() = default;
OptionalTrustTokenParams::OptionalTrustTokenParams(std::nullopt_t) {}
OptionalTrustTokenParams::OptionalTrustTokenParams(
    mojom::TrustTokenParamsPtr ptr)
    : ptr_(std::move(ptr)) {}
OptionalTrustTokenParams::OptionalTrustTokenParams(
    const mojom::TrustTokenParams& params)
    : ptr_(params.Clone()) {}
OptionalTrustTokenParams::OptionalTrustTokenParams(
    const OptionalTrustTokenParams& other) {
  ptr_ = other.as_ptr().Clone();
}
OptionalTrustTokenParams& OptionalTrustTokenParams::operator=(
    const OptionalTrustTokenParams& other) {
  ptr_ = other.as_ptr().Clone();
  return *this;
}
OptionalTrustTokenParams::OptionalTrustTokenParams(
    OptionalTrustTokenParams&& other) = default;
OptionalTrustTokenParams& OptionalTrustTokenParams::operator=(
    OptionalTrustTokenParams&& other) = default;
OptionalTrustTokenParams::~OptionalTrustTokenParams() = default;
bool OptionalTrustTokenParams::operator==(
    const OptionalTrustTokenParams& other) const {
  return mojo::Equals(ptr_, other.ptr_);
}

}  // namespace network
