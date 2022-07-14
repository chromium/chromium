// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/cert_verifier/test_cert_verifier_service_factory.h"

#include <memory>
#include <type_traits>

#include "base/memory/scoped_refptr.h"
#include "net/net_buildflags.h"
#include "services/cert_verifier/cert_verifier_service.h"
#include "services/cert_verifier/cert_verifier_service_factory.h"

namespace cert_verifier {

TestCertVerifierServiceFactoryImpl::GetNewCertVerifierParams::
    GetNewCertVerifierParams() = default;
TestCertVerifierServiceFactoryImpl::GetNewCertVerifierParams::
    GetNewCertVerifierParams(GetNewCertVerifierParams&&) = default;
TestCertVerifierServiceFactoryImpl::GetNewCertVerifierParams&
TestCertVerifierServiceFactoryImpl::GetNewCertVerifierParams::operator=(
    TestCertVerifierServiceFactoryImpl::GetNewCertVerifierParams&& other) =
    default;
TestCertVerifierServiceFactoryImpl::GetNewCertVerifierParams::
    ~GetNewCertVerifierParams() = default;

TestCertVerifierServiceFactoryImpl::TestCertVerifierServiceFactoryImpl() =
    default;

TestCertVerifierServiceFactoryImpl::~TestCertVerifierServiceFactoryImpl() =
    default;

void TestCertVerifierServiceFactoryImpl::GetNewCertVerifier(
    mojo::PendingReceiver<mojom::CertVerifierService> receiver,
    mojom::CertVerifierCreationParamsPtr creation_params) {
  if (!delegate_) {
    InitDelegate();
  }

  GetNewCertVerifierParams params;
  params.receiver = std::move(receiver);
  params.creation_params = std::move(creation_params);

  captured_params_.push_front(std::move(params));
}

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
void TestCertVerifierServiceFactoryImpl::UpdateChromeRootStore(
    mojom::ChromeRootStorePtr new_root_store) {}

void TestCertVerifierServiceFactoryImpl::GetChromeRootStoreInfo(
    GetChromeRootStoreInfoCallback callback) {
  mojom::ChromeRootStoreInfoPtr info_ptr = mojom::ChromeRootStoreInfo::New();
  info_ptr->version = 42;
  std::move(callback).Run(std::move(info_ptr));
}
#endif

void TestCertVerifierServiceFactoryImpl::ReleaseAllCertVerifierParams() {
  DCHECK(delegate_);
  while (!captured_params_.empty())
    ReleaseNextCertVerifierParams();
}

void TestCertVerifierServiceFactoryImpl::ReleaseNextCertVerifierParams() {
  DCHECK(delegate_);
  GetNewCertVerifierParams params = std::move(captured_params_.back());
  captured_params_.pop_back();
  delegate_remote_->GetNewCertVerifier(std::move(params.receiver),
                                       std::move(params.creation_params));
}

void TestCertVerifierServiceFactoryImpl::InitDelegate() {
  delegate_ = std::make_unique<CertVerifierServiceFactoryImpl>(
      delegate_remote_.BindNewPipeAndPassReceiver());
}

}  // namespace cert_verifier
