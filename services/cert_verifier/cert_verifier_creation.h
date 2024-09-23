// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_CERT_VERIFIER_CERT_VERIFIER_CREATION_H_
#define SERVICES_CERT_VERIFIER_CERT_VERIFIER_CREATION_H_

#include <memory>

#include "base/component_export.h"
#include "base/memory/scoped_refptr.h"
#include "net/cert/cert_net_fetcher.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/cert_verify_proc.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"
#include "services/network/public/mojom/cert_verifier_service_updater.mojom.h"

// Set of utility functions to help with creation of CertVerifiers for
// CertVerifyServiceFactory.
namespace cert_verifier {

// Certain platforms and build configurations require a net::CertNetFetcher in
// order to instantiate a net::CertVerifier. Callers of CreateCertVerifier() can
// call IsUsingCertNetFetcher() to decide whether or not to pass a
// net::CertNetFetcher.
bool IsUsingCertNetFetcher();

// Creates a concrete net::CertVerifier based on the platform and the particular
// build configuration. |creation_params| and |root_store_data| are optional.
std::unique_ptr<net::CertVerifierWithUpdatableProc> CreateCertVerifier(
    mojom::CertVerifierCreationParams* creation_params,
    scoped_refptr<net::CertNetFetcher> cert_net_fetcher,
    const net::CertVerifyProc::ImplParams& impl_params,
    const net::CertVerifyProc::InstanceParams& instance_params);

// Update the |instance_params| for the verifier based on the contents of
// |additional_certificates|.
void UpdateCertVerifierInstanceParams(
    const mojom::AdditionalCertificatesPtr& additional_certificates,
    net::CertVerifyProc::InstanceParams* instance_params);

}  // namespace cert_verifier

#endif  // SERVICES_CERT_VERIFIER_CERT_VERIFIER_CREATION_H_
