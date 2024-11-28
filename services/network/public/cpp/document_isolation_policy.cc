// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/document_isolation_policy.h"

namespace network {

DocumentIsolationPolicy::DocumentIsolationPolicy() = default;
DocumentIsolationPolicy::DocumentIsolationPolicy(
    const DocumentIsolationPolicy& src) = default;
DocumentIsolationPolicy::DocumentIsolationPolicy(
    DocumentIsolationPolicy&& src) = default;
DocumentIsolationPolicy::~DocumentIsolationPolicy() = default;
DocumentIsolationPolicy::DocumentIsolationPolicy(
    mojom::DocumentIsolationPolicyValue value)
    : value(value) {}

DocumentIsolationPolicy& DocumentIsolationPolicy::operator=(
    const DocumentIsolationPolicy& src) = default;
DocumentIsolationPolicy& DocumentIsolationPolicy::operator=(
    DocumentIsolationPolicy&& src) = default;
bool DocumentIsolationPolicy::operator==(
    const DocumentIsolationPolicy& other) const = default;

bool DIPCompatibleWithCrossOriginIsolated(const DocumentIsolationPolicy& dip) {
  return DIPCompatibleWithCrossOriginIsolated(dip.value);
}

bool DIPCompatibleWithCrossOriginIsolated(
    mojom::DocumentIsolationPolicyValue value) {
  switch (value) {
    case mojom::DocumentIsolationPolicyValue::kNone:
      return false;
    case mojom::DocumentIsolationPolicyValue::kIsolateAndCredentialless:
    case mojom::DocumentIsolationPolicyValue::kIsolateAndRequireCorp:
      return true;
  }
}

}  // namespace network
