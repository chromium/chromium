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

}  // namespace network
