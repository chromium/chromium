// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_cert_verifier_service_factory.h"

#include <memory>
#include <type_traits>

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "net/net_buildflags.h"
#include "services/cert_verifier/cert_verifier_service.h"
#include "services/cert_verifier/cert_verifier_service_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

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
    mojo::PendingRemote<mojom::CertVerifierServiceClient> client,
    mojom::CertVerifierCreationParamsPtr creation_params) {
  if (!delegate_) {
    InitDelegate();
  }

  GetNewCertVerifierParams params;
  params.receiver = std::move(receiver);
  params.client = std::move(client);
  params.creation_params = std::move(creation_params);

  captured_params_.push_front(std::move(params));
}

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
void TestCertVerifierServiceFactoryImpl::UpdateChromeRootStore(
    mojom::ChromeRootStorePtr new_root_store,
    UpdateChromeRootStoreCallback callback) {
  std::move(callback).Run();
}

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
                                       std::move(params.client),
                                       std::move(params.creation_params));
}

void TestCertVerifierServiceFactoryImpl::InitDelegate() {
  delegate_ = base::MakeRefCounted<DelegateOwner>(
#if BUILDFLAG(IS_CHROMEOS)
      // In-process CertVerifierService in Ash and Lacros should run on the IO
      // thread because it interacts with IO-bound NSS and ChromeOS user slots.
      // See for example InitializeNSSForChromeOSUser() or
      // CertDbInitializerIOImpl.
      content::GetIOThreadTaskRunner({})
#else
      base::SequencedTaskRunner::GetCurrentDefault()
#endif
  );
  delegate_->Init(delegate_remote_.BindNewPipeAndPassReceiver());
}

TestCertVerifierServiceFactoryImpl::DelegateOwner::DelegateOwner(
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : base::RefCountedDeleteOnSequence<DelegateOwner>(std::move(task_runner)) {}

TestCertVerifierServiceFactoryImpl::DelegateOwner::~DelegateOwner() = default;

void TestCertVerifierServiceFactoryImpl::DelegateOwner::Init(
    mojo::PendingReceiver<cert_verifier::mojom::CertVerifierServiceFactory>
        receiver) {
  if (!owning_task_runner()->RunsTasksInCurrentSequence()) {
    owning_task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&DelegateOwner::Init, this, std::move(receiver)));
    return;
  }
  delegate_ =
      std::make_unique<CertVerifierServiceFactoryImpl>(std::move(receiver));
}

}  // namespace cert_verifier
