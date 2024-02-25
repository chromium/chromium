// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_OPTIONAL_TRUST_TOKEN_PARAMS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_OPTIONAL_TRUST_TOKEN_PARAMS_H_

#include <optional>

#include "base/component_export.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"

namespace network {

// This class exists to wrap mojom::TrustTokenParamsPtr for use as a field in
// network::ResourceRequest.
//
// Motivation:
// 1. network::ResourceRequest has a requirement that all of
// network::ResourceRequest's members be trivially copyable;
// 2. Mojo struct pointers aren't, by default, trivially copyable;
// 3. Mojo only knows, from its generated code, how to serialize and deserialize
// struct pointers, not raw Mojo structs.
//
// One solution to this dilemma would be to manually define separate Mojo
// StructTraits for the raw struct type (network::mojom::TrustTokenParams), but
// this would add maintenance burden since it would require updating the traits
// every time the structure's definition changes.
//
// Using this trivially-copyable wrapper class (where the copy constructor and
// copy assignment operators use mojo::Clone) allows changing the format of the
// Mojo struct without having to manually update the corresponding
// serialization/deserialization code.
class COMPONENT_EXPORT(NETWORK_CPP_BASE) OptionalTrustTokenParams {
 public:
  // The constructors Match std::optional to the extent possible.
  OptionalTrustTokenParams();
  OptionalTrustTokenParams(std::nullopt_t);  // NOLINT
  explicit OptionalTrustTokenParams(mojom::TrustTokenParamsPtr);

  // Copy assignment uses mojo::Clone.
  OptionalTrustTokenParams(const mojom::TrustTokenParams&);  // NOLINT
  OptionalTrustTokenParams(const OptionalTrustTokenParams&);
  OptionalTrustTokenParams& operator=(const OptionalTrustTokenParams&);
  OptionalTrustTokenParams(OptionalTrustTokenParams&&);
  OptionalTrustTokenParams& operator=(OptionalTrustTokenParams&&);

  ~OptionalTrustTokenParams();

  // This comparison operator wraps mojo::Equals.
  bool operator==(const OptionalTrustTokenParams&) const;
  bool operator!=(const OptionalTrustTokenParams& rhs) const {
    return !(*this == rhs);
  }

  explicit operator bool() const { return has_value(); }
  bool has_value() const { return !!ptr_; }

  mojom::TrustTokenParams& value() {
    CHECK(has_value());
    return *ptr_;
  }

  const mojom::TrustTokenParams& value() const {
    CHECK(has_value());
    return *ptr_;
  }

  const mojom::TrustTokenParams* operator->() const {
    CHECK(has_value());
    return ptr_.get();
  }

  mojom::TrustTokenParams* operator->() {
    CHECK(has_value());
    return ptr_.get();
  }

  // |as_ptr| returns null if this object is empty.
  const mojom::TrustTokenParamsPtr& as_ptr() const { return ptr_; }
  mojom::TrustTokenParamsPtr& as_ptr() { return ptr_; }

 private:
  mojom::TrustTokenParamsPtr ptr_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_OPTIONAL_TRUST_TOKEN_PARAMS_H_
