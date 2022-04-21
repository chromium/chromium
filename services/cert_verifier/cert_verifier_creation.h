// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_CERT_VERIFIER_CERT_VERIFIER_CREATION_H_
#define SERVICES_CERT_VERIFIER_CERT_VERIFIER_CREATION_H_

#include <memory>

#include "base/component_export.h"
#include "base/memory/scoped_refptr.h"
#include "net/cert/cert_net_fetcher.h"
#include "net/cert/cert_verifier.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"

namespace cert_verifier {

// Certain platforms and build configurations require a net::CertNetFetcher in
// order to instantiate a net::CertVerifier. Callers of CreateCertVerifier() can
// call IsUsingCertNetFetcher() to decide whether or not to pass a
// net::CertNetFetcher.
bool IsUsingCertNetFetcher();

// Creates a concrete net::CertVerifier based on the platform and the particular
// build configuration. |creation_params| is optional.
std::unique_ptr<net::CertVerifier> CreateCertVerifier(
    mojom::CertVerifierCreationParams* creation_params,
    scoped_refptr<net::CertNetFetcher> cert_net_fetcher);

}  // namespace cert_verifier

#endif  // SERVICES_CERT_VERIFIER_CERT_VERIFIER_CREATION_H_
