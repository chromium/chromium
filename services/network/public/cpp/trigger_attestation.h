// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_TRIGGER_ATTESTATION_H_
#define SERVICES_NETWORK_PUBLIC_CPP_TRIGGER_ATTESTATION_H_

#include <string>

#include "base/component_export.h"
#include "base/guid.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace network {

class COMPONENT_EXPORT(NETWORK_CPP_TRIGGER_ATTESTATION) TriggerAttestation {
 public:
  // Creates a TriggerAttestation instance if the `aggregatable_report_id` is a
  // valid id and `token` is not empty.
  static absl::optional<TriggerAttestation> Create(
      std::string token,
      const std::string& aggregatable_report_id);

  // Creates an invalid instance for use with Mojo deserialization, which
  // requires types to be default-constructible.
  // TODO(https://crbug.com/1408442): Avoid exposing default constructor
  TriggerAttestation();

  ~TriggerAttestation();

  TriggerAttestation(const TriggerAttestation&);
  TriggerAttestation& operator=(const TriggerAttestation&);

  TriggerAttestation(TriggerAttestation&&);
  TriggerAttestation& operator=(TriggerAttestation&&);

  const std::string& token() const { return token_; }
  const base::GUID& aggregatable_report_id() const {
    return aggregatable_report_id_;
  }

 private:
  TriggerAttestation(std::string token, base::GUID aggregatable_report_id);

  std::string token_;
  base::GUID aggregatable_report_id_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_TRIGGER_ATTESTATION_H_
