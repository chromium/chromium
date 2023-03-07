// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/cert_verifier/cert_verifier_service_factory.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/types/optional_util.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/features.h"
#include "net/cert/cert_net_fetcher.h"
#include "net/cert/cert_verifier.h"
#include "net/net_buildflags.h"
#include "services/cert_verifier/cert_net_url_loader/cert_net_fetcher_url_loader.h"
#include "services/cert_verifier/cert_verifier_service.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
#include "mojo/public/cpp/base/big_buffer.h"
#include "net/cert/internal/trust_store_chrome.h"
#include "net/cert/pki/parse_name.h"
#include "net/cert/pki/parsed_certificate.h"
#include "net/cert/root_store_proto_lite/root_store.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#endif

namespace net {
class ChromeRootStoreData;
}
namespace cert_verifier {
namespace {

internal::CertVerifierServiceImpl* GetNewCertVerifierImpl(
    mojom::CertVerifierServiceParams* impl_params,
    mojo::PendingReceiver<mojom::CertVerifierService> receiver,
    mojom::CertVerifierCreationParamsPtr creation_params,
    const net::ChromeRootStoreData* root_store_data,
    scoped_refptr<CertNetFetcherURLLoader>* out_cert_net_fetcher) {
  scoped_refptr<CertNetFetcherURLLoader> cert_net_fetcher;

  // Sometimes the cert_net_fetcher isn't used by CreateCertVerifier.
  // But losing the last ref without calling Shutdown() will cause a CHECK
  // failure, so keep a ref.
  if (IsUsingCertNetFetcher()) {
    cert_net_fetcher = base::MakeRefCounted<CertNetFetcherURLLoader>();
  }

  std::unique_ptr<net::CertVerifierWithUpdatableProc> cert_verifier =
      CreateCertVerifier(impl_params, creation_params.get(), cert_net_fetcher,
                         root_store_data);

  // As an optimization, if the CertNetFetcher isn't used by the CertVerifier,
  // shut it down immediately.
  if (cert_net_fetcher && cert_net_fetcher->HasOneRef()) {
    cert_net_fetcher->Shutdown();
    cert_net_fetcher.reset();
  }

  // Return reference to cert_net_fetcher for testing purposes.
  if (out_cert_net_fetcher)
    *out_cert_net_fetcher = cert_net_fetcher;

  // The service will delete itself upon disconnection.
  return new internal::CertVerifierServiceImpl(std::move(cert_verifier),
                                               std::move(receiver),
                                               std::move(cert_net_fetcher));
}

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
std::string GetName(std::shared_ptr<const net::ParsedCertificate> cert) {
  net::RDNSequence subject_rdn;
  if (!net::ParseName(cert->subject_tlv(), &subject_rdn)) {
    return "UNKNOWN";
  }
  std::string subject_string;
  if (!net::ConvertToRFC2253(subject_rdn, &subject_string)) {
    return "UNKNOWN";
  }
  return subject_string;
}

std::string GetHash(std::shared_ptr<const net::ParsedCertificate> cert) {
  net::SHA256HashValue hash =
      net::X509Certificate::CalculateFingerprint256(cert->cert_buffer());
  return base::HexEncode(hash.data, std::size(hash.data));
}
#endif

}  // namespace

CertVerifierServiceFactoryImpl::CertVerifierServiceFactoryImpl(
    mojom::CertVerifierServiceParamsPtr params,
    mojo::PendingReceiver<mojom::CertVerifierServiceFactory> receiver)
    : service_params_(std::move(params)), receiver_(this, std::move(receiver)) {
  if (!service_params_) {
    service_params_ = mojom::CertVerifierServiceParams::New();
#if BUILDFLAG(CHROME_ROOT_STORE_OPTIONAL)
    service_params_->use_chrome_root_store =
        base::FeatureList::IsEnabled(net::features::kChromeRootStoreUsed);
#endif
  }
}

CertVerifierServiceFactoryImpl::~CertVerifierServiceFactoryImpl() = default;

void CertVerifierServiceFactoryImpl::GetNewCertVerifier(
    mojo::PendingReceiver<mojom::CertVerifierService> receiver,
    mojom::CertVerifierCreationParamsPtr creation_params) {
  net::ChromeRootStoreData* root_store_data = nullptr;
#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
  root_store_data = base::OptionalToPtr(root_store_data_);
#endif

  internal::CertVerifierServiceImpl* service_impl =
      GetNewCertVerifierImpl(service_params_.get(), std::move(receiver),
                             std::move(creation_params), root_store_data,
                             /*out_cert_net_fetcher=*/nullptr);

  verifier_services_.insert(service_impl);
  service_impl->SetCertVerifierServiceFactory(weak_factory_.GetWeakPtr());
}

void CertVerifierServiceFactoryImpl::GetServiceParamsForTesting(
    GetServiceParamsForTestingCallback callback) {
  std::move(callback).Run(service_params_.Clone());
}

void CertVerifierServiceFactoryImpl::GetNewCertVerifierForTesting(
    mojo::PendingReceiver<mojom::CertVerifierService> receiver,
    mojom::CertVerifierCreationParamsPtr creation_params,
    scoped_refptr<CertNetFetcherURLLoader>* cert_net_fetcher_ptr) {
  GetNewCertVerifierImpl(service_params_.get(), std::move(receiver),
                         std::move(creation_params),
                         /*root_store_data=*/nullptr, cert_net_fetcher_ptr);
}

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
void CertVerifierServiceFactoryImpl::UpdateChromeRootStore(
    mojom::ChromeRootStorePtr new_root_store) {
  if (new_root_store->serialized_proto_root_store.size() == 0) {
    LOG(ERROR) << "Empty serialized RootStore proto";
    return;
  }

  chrome_root_store::RootStore proto;
  if (!proto.ParseFromArray(
          new_root_store->serialized_proto_root_store.data(),
          new_root_store->serialized_proto_root_store.size())) {
    LOG(ERROR) << "error parsing proto for Chrome Root Store";
    return;
  }

  // We only check against the compiled version to allow for us to to use
  // Component Updater to revert to older versions. Check is left in
  // to guard against Component updater being stuck on older versions due
  // to daily updates of the PKI Metadata component being broken.
  if (proto.version_major() <= net::CompiledChromeRootStoreVersion()) {
    return;
  }

  absl::optional<net::ChromeRootStoreData> root_store_data =
      net::ChromeRootStoreData::CreateChromeRootStoreData(proto);
  if (!root_store_data) {
    LOG(ERROR) << "error interpreting proto for Chrome Root Store";
    return;
  }

  if (root_store_data->anchors().empty()) {
    LOG(ERROR) << "parsed root store contained no anchors";
    return;
  }

  for (internal::CertVerifierServiceImpl* service : verifier_services_) {
    service->UpdateChromeRootStoreData(&root_store_data.value());
  }

  // Update the stored Chrome Root Store so that new CertVerifierService
  // instances will start with the updated store.
  root_store_data_ = std::move(root_store_data);
}

void CertVerifierServiceFactoryImpl::GetChromeRootStoreInfo(
    GetChromeRootStoreInfoCallback callback) {
  mojom::ChromeRootStoreInfoPtr info_ptr = mojom::ChromeRootStoreInfo::New();
  if (root_store_data_) {
    info_ptr->version = root_store_data_->version();
    for (auto cert : root_store_data_->anchors()) {
      info_ptr->root_cert_info.push_back(
          mojom::ChromeRootCertInfo::New(GetName(cert), GetHash(cert)));
    }
  } else {
    info_ptr->version = net::CompiledChromeRootStoreVersion();
    for (auto cert : net::CompiledChromeRootStoreAnchors()) {
      info_ptr->root_cert_info.push_back(
          mojom::ChromeRootCertInfo::New(GetName(cert), GetHash(cert)));
    }
  }
  std::move(callback).Run(std::move(info_ptr));
}
#endif

void CertVerifierServiceFactoryImpl::RemoveService(
    internal::CertVerifierServiceImpl* service_impl) {
  verifier_services_.erase(service_impl);
}

}  // namespace cert_verifier
