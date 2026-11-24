// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_CERT_VERIFIER_PUBLIC_CPP_COMPONENT_DATA_TYPE_H_
#define SERVICES_CERT_VERIFIER_PUBLIC_CPP_COMPONENT_DATA_TYPE_H_

#include "base/containers/enum_set.h"
#include "net/net_buildflags.h"
#include "services/network/public/cpp/network_service_buildflags.h"

namespace cert_verifier {

// Identifies a component-updated PKI data source that
// CertVerifierServiceFactoryImpl can wait for before processing certificate
// verifications.
enum class ComponentDataType {
#if BUILDFLAG(IS_CT_SUPPORTED) || BUILDFLAG(CHROME_KEY_PINNING_SUPPORTED) || \
    BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
  kPKIMetadata,
#endif
#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
  kMtcMetadata,
#endif
  kCRLSet,
  kMinValue = 0,
  kMaxValue = kCRLSet,
};

using ComponentDataTypeSet = base::EnumSet<ComponentDataType>;

}  // namespace cert_verifier

#endif  // SERVICES_CERT_VERIFIER_PUBLIC_CPP_COMPONENT_DATA_TYPE_H_
