// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_TEST_CERT_VERIFIER_SERVICE_FACTORY_H_
#define CONTENT_PUBLIC_TEST_TEST_CERT_VERIFIER_SERVICE_FACTORY_H_

#include <memory>

#include "base/containers/circular_deque.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/task/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/net_buildflags.h"
#include "services/cert_verifier/cert_verifier_service_factory.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"
#include "services/network/public/mojom/cert_verifier_service.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace cert_verifier {

// Captures the params passed to GetNewCertVerifier, and sends them to a wrapped
// CertVerifierServiceFactoryImpl when instructed to.
class TestCertVerifierServiceFactoryImpl
    : public mojom::CertVerifierServiceFactory {
 public:
  TestCertVerifierServiceFactoryImpl();
  ~TestCertVerifierServiceFactoryImpl() override;

  struct GetNewCertVerifierParams {
    GetNewCertVerifierParams();
    GetNewCertVerifierParams(GetNewCertVerifierParams&&);
    GetNewCertVerifierParams& operator=(GetNewCertVerifierParams&& other);
    GetNewCertVerifierParams(const GetNewCertVerifierParams&) = delete;
    GetNewCertVerifierParams& operator=(const GetNewCertVerifierParams&) =
        delete;
    ~GetNewCertVerifierParams();

    mojo::PendingReceiver<mojom::CertVerifierService> receiver;
    mojom::CertVerifierCreationParamsPtr creation_params;
  };

  // mojom::CertVerifierServiceFactory implementation:
  void GetNewCertVerifier(
      mojo::PendingReceiver<mojom::CertVerifierService> receiver,
      mojom::CertVerifierCreationParamsPtr creation_params) override;
  void GetServiceParamsForTesting(
      GetServiceParamsForTestingCallback callback) override;

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
  void UpdateChromeRootStore(mojom::ChromeRootStorePtr new_root_store) override;
  void GetChromeRootStoreInfo(GetChromeRootStoreInfoCallback callback) override;
#endif

  // Pops the first request off the back of the list and forwards it to the
  // delegate CertVerifierServiceFactory.
  void ReleaseNextCertVerifierParams();
  void ReleaseAllCertVerifierParams();

  size_t num_captured_params() const { return captured_params_.size(); }
  // Ordered from most recent to least recent.
  const GetNewCertVerifierParams* GetParamsAtIndex(int i) {
    return &captured_params_[i];
  }

 private:
  class DelegateOwner : public base::RefCountedDeleteOnSequence<DelegateOwner> {
   public:
    explicit DelegateOwner(
        scoped_refptr<base::SequencedTaskRunner> owning_task_runner);

    void Init(
        mojom::CertVerifierServiceParamsPtr params,
        mojo::PendingReceiver<cert_verifier::mojom::CertVerifierServiceFactory>
            receiver);

   private:
    friend class base::RefCountedDeleteOnSequence<DelegateOwner>;
    friend class base::DeleteHelper<DelegateOwner>;

    ~DelegateOwner();

    std::unique_ptr<CertVerifierServiceFactoryImpl> delegate_;
  };

  void InitDelegate();

  mojo::Remote<mojom::CertVerifierServiceFactory> delegate_remote_;
  scoped_refptr<DelegateOwner> delegate_;

  base::circular_deque<GetNewCertVerifierParams> captured_params_;
};

}  // namespace cert_verifier

#endif  // CONTENT_PUBLIC_TEST_TEST_CERT_VERIFIER_SERVICE_FACTORY_H_
