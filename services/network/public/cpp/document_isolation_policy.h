// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_DOCUMENT_ISOLATION_POLICY_H_
#define SERVICES_NETWORK_PUBLIC_CPP_DOCUMENT_ISOLATION_POLICY_H_

#include <optional>
#include <string>

#include "base/component_export.h"
#include "services/network/public/mojom/document_isolation_policy.mojom-shared.h"

namespace network {

// This corresponds to network::mojom::DocumentIsolationPolicy.
// See the comments there.
struct COMPONENT_EXPORT(NETWORK_CPP_DOCUMENT_ISOLATION)
    DocumentIsolationPolicy final {
  DocumentIsolationPolicy();
  DocumentIsolationPolicy(const DocumentIsolationPolicy&);
  DocumentIsolationPolicy(DocumentIsolationPolicy&&);
  explicit DocumentIsolationPolicy(mojom::DocumentIsolationPolicyValue);
  DocumentIsolationPolicy& operator=(const DocumentIsolationPolicy&);
  DocumentIsolationPolicy& operator=(DocumentIsolationPolicy&&);
  ~DocumentIsolationPolicy();
  bool operator==(const DocumentIsolationPolicy&) const;

  mojom::DocumentIsolationPolicyValue value =
      mojom::DocumentIsolationPolicyValue::kNone;
  std::optional<std::string> reporting_endpoint;
  mojom::DocumentIsolationPolicyValue report_only_value =
      mojom::DocumentIsolationPolicyValue::kNone;
  std::optional<std::string> report_only_reporting_endpoint;
};

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_DOCUMENT_ISOLATION_POLICY_H_
