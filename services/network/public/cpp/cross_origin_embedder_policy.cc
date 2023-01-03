// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cross_origin_embedder_policy.h"


namespace network {

CrossOriginEmbedderPolicy::CrossOriginEmbedderPolicy() = default;
CrossOriginEmbedderPolicy::CrossOriginEmbedderPolicy(
    const CrossOriginEmbedderPolicy& src) = default;
CrossOriginEmbedderPolicy::CrossOriginEmbedderPolicy(
    CrossOriginEmbedderPolicy&& src) = default;
CrossOriginEmbedderPolicy::~CrossOriginEmbedderPolicy() = default;
CrossOriginEmbedderPolicy::CrossOriginEmbedderPolicy(
    mojom::CrossOriginEmbedderPolicyValue value)
    : value(value) {}

CrossOriginEmbedderPolicy& CrossOriginEmbedderPolicy::operator=(
    const CrossOriginEmbedderPolicy& src) = default;
CrossOriginEmbedderPolicy& CrossOriginEmbedderPolicy::operator=(
    CrossOriginEmbedderPolicy&& src) = default;
bool CrossOriginEmbedderPolicy::operator==(
    const CrossOriginEmbedderPolicy& other) const {
  return value == other.value &&
         reporting_endpoint == other.reporting_endpoint &&
         report_only_value == other.report_only_value &&
         report_only_reporting_endpoint == other.report_only_reporting_endpoint;
}

bool CompatibleWithCrossOriginIsolated(const CrossOriginEmbedderPolicy& coep) {
  return CompatibleWithCrossOriginIsolated(coep.value);
}

// [spec]:
// https://html.spec.whatwg.org/C/#compatible-with-cross-origin-isolation An
// embedder policy value is compatible with cross-origin isolation if it is
// "credentialless" or "require-corp".
bool CompatibleWithCrossOriginIsolated(
    mojom::CrossOriginEmbedderPolicyValue value) {
  switch (value) {
    case mojom::CrossOriginEmbedderPolicyValue::kNone:
      return false;
    case mojom::CrossOriginEmbedderPolicyValue::kCredentialless:
    case mojom::CrossOriginEmbedderPolicyValue::kRequireCorp:
      return true;
  }
}

}  // namespace network
