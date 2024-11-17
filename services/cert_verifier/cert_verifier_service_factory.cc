// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/cert_verifier/cert_verifier_service_factory.h"

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/thread_pool.h"
#include "base/types/optional_util.h"
#include "build/build_config.h"
#include "crypto/sha2.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/features.h"
#include "net/base/ip_address.h"
#include "net/cert/cert_net_fetcher.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/crl_set.h"
#include "net/cert/internal/platform_trust_store.h"
#include "net/cert/internal/system_trust_store.h"
#include "net/cert/internal/trust_store_chrome.h"
#include "net/cert/x509_util.h"
#include "net/net_buildflags.h"
#include "services/cert_verifier/cert_net_url_loader/cert_net_fetcher_url_loader.h"
#include "services/cert_verifier/cert_verifier_creation.h"
#include "services/cert_verifier/cert_verifier_service.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

#if BUILDFLAG(IS_CT_SUPPORTED)
#include "components/certificate_transparency/chrome_ct_policy_enforcer.h"
#include "services/network/public/mojom/ct_log_info.mojom.h"
#endif

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
#include <optional>

#include "base/version_info/version_info.h"  // nogncheck
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/base/proto_wrapper.h"
#include "net/cert/internal/trust_store_chrome.h"
#include "net/cert/root_store_proto_lite/root_store.pb.h"
#include "third_party/boringssl/src/pki/parse_name.h"
#include "third_party/boringssl/src/pki/parsed_certificate.h"
#endif

namespace net {
class ChromeRootStoreData;
}
namespace cert_verifier {
namespace {

internal::CertVerifierServiceImpl* GetNewCertVerifierImpl(
    mojo::PendingReceiver<mojom::CertVerifierService> service_receiver,
    mojo::PendingReceiver<mojom::CertVerifierServiceUpdater> updater_receiver,
    mojo::PendingRemote<mojom::CertVerifierServiceClient> client,
    mojom::CertVerifierCreationParamsPtr creation_params,
    const net::CertVerifyProc::ImplParams& impl_params,
    scoped_refptr<CertNetFetcherURLLoader>* out_cert_net_fetcher) {
  scoped_refptr<CertNetFetcherURLLoader> cert_net_fetcher;

  // Sometimes the cert_net_fetcher isn't used by CreateCertVerifier.
  // But losing the last ref without calling Shutdown() will cause a CHECK
  // failure, so keep a ref.
  if (IsUsingCertNetFetcher()) {
    cert_net_fetcher = base::MakeRefCounted<CertNetFetcherURLLoader>();
  }

  // Populate initial instance params from creation params.
  net::CertVerifyProc::InstanceParams instance_params;
  if (creation_params->initial_additional_certificates) {
    UpdateCertVerifierInstanceParams(
        creation_params->initial_additional_certificates, &instance_params);
  }

  std::unique_ptr<net::CertVerifierWithUpdatableProc> cert_verifier =
      CreateCertVerifier(creation_params.get(), cert_net_fetcher, impl_params,
                         instance_params);

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
  return new internal::CertVerifierServiceImpl(
      std::move(cert_verifier), std::move(service_receiver),
      std::move(updater_receiver), std::move(client),
      std::move(cert_net_fetcher), std::move(instance_params),
      creation_params->wait_for_update);
}

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
std::string GetHash(const bssl::ParsedCertificate& cert) {
  net::SHA256HashValue hash =
      net::X509Certificate::CalculateFingerprint256(cert.cert_buffer());
  return base::HexEncode(hash.data);
}

bool IsVersionConstraintSatisified(
    const net::ChromeRootCertConstraints constraint) {
  if (constraint.min_version.has_value() &&
      version_info::GetVersion() < constraint.min_version.value()) {
    return false;
  }

  if (constraint.max_version_exclusive.has_value() &&
      version_info::GetVersion() >= constraint.max_version_exclusive.value()) {
    return false;
  }

  return true;
}

// we only check any version constraints, as we don't have a certificate here to
// check any SCT constraints.
bool IsAnchorTrustedOnThisChromeVersion(
    const net::ChromeRootStoreData::Anchor& anchor) {
  if (anchor.constraints.empty()) {
    return true;
  }

  for (const auto& constraint : anchor.constraints) {
    if (IsVersionConstraintSatisified(constraint)) {
      return true;
    }
  }
  return false;
}
#endif

// Attempts to parse |crl_set|, returning nullptr on error or the parsed
// CRLSet.
scoped_refptr<net::CRLSet> ParseCRLSet(mojo_base::BigBuffer crl_set) {
  scoped_refptr<net::CRLSet> result;
  // The BigBuffer comes from a trusted process, so we don't need to copy the
  // data out before parsing.
  if (!net::CRLSet::Parse(
          std::string_view(reinterpret_cast<const char*>(crl_set.data()),
                           crl_set.size()),
          &result)) {
    return nullptr;
  }
  return result;
}

#if BUILDFLAG(IS_CT_SUPPORTED)
// Filters `log_list` for disqualified logs, returning them as sorted vectors
// in `disqualified_logs`. Stores the operator history and log type of all logs
// in `log_info`, suitable for use with a `CTPolicyEnforcer`.
void ComputeCTLogInfo(
    const std::vector<network::mojom::CTLogInfoPtr>& log_list,
    std::vector<std::pair<std::string, base::Time>>* disqualified_logs,
    std::map<std::string, certificate_transparency::LogInfo>* log_info) {
  for (const auto& log : log_list) {
    std::string log_id = crypto::SHA256HashString(log->public_key);
    if (log->disqualified_at) {
      disqualified_logs->emplace_back(log_id, log->disqualified_at.value());
    }
    certificate_transparency::OperatorHistoryEntry entry;
    entry.current_operator_ = log->current_operator;
    for (const auto& previous_operator : log->previous_operators) {
      entry.previous_operators_.emplace_back(previous_operator->name,
                                             previous_operator->end_time);
    }
    certificate_transparency::LogInfo info;
    info.operator_history = entry;
    info.log_type = log->log_type;
    (*log_info)[log_id] = info;
  }

  std::sort(std::begin(*disqualified_logs), std::end(*disqualified_logs));
}

#endif

}  // namespace

CertVerifierServiceFactoryImpl::CertVerifierServiceFactoryImpl(
    mojo::PendingReceiver<mojom::CertVerifierServiceFactory> receiver)
    : receiver_(this, std::move(receiver)) {}

CertVerifierServiceFactoryImpl::~CertVerifierServiceFactoryImpl() = default;

void CertVerifierServiceFactoryImpl::GetNewCertVerifier(
    mojo::PendingReceiver<mojom::CertVerifierService> service_receiver,
    mojo::PendingReceiver<mojom::CertVerifierServiceUpdater> updater_receiver,
    mojo::PendingRemote<mojom::CertVerifierServiceClient> client,
    mojom::CertVerifierCreationParamsPtr creation_params) {
  internal::CertVerifierServiceImpl* service_impl = GetNewCertVerifierImpl(
      std::move(service_receiver), std::move(updater_receiver),
      std::move(client), std::move(creation_params), proc_params_,
      /*out_cert_net_fetcher=*/nullptr);

  verifier_services_.insert(service_impl);
  service_impl->SetCertVerifierServiceFactory(weak_factory_.GetWeakPtr());
}

void CertVerifierServiceFactoryImpl::GetNewCertVerifierForTesting(
    mojo::PendingReceiver<mojom::CertVerifierService> service_receiver,
    mojo::PendingReceiver<mojom::CertVerifierServiceUpdater> updater_receiver,
    mojo::PendingRemote<mojom::CertVerifierServiceClient> client,
    mojom::CertVerifierCreationParamsPtr creation_params,
    scoped_refptr<CertNetFetcherURLLoader>* cert_net_fetcher_ptr) {
  GetNewCertVerifierImpl(std::move(service_receiver),
                         std::move(updater_receiver), std::move(client),
                         std::move(creation_params), proc_params_,
                         cert_net_fetcher_ptr);
}

void CertVerifierServiceFactoryImpl::UpdateCRLSet(
    mojo_base::BigBuffer crl_set,
    mojom::CertVerifierServiceFactory::UpdateCRLSetCallback callback) {
  // Posting to thread pool might fail if the browser is in shutdown. Wrap the
  // callback so that it will be invoked anyway to avoid violating Mojo
  // expectations. This is a little misleading since the CRLSet was not
  // actually updated, but if the browser is shutting down then it doesn't
  // really matter. (If it actually mattered the callback could get passed a
  // boolean success value or something.)
  callback = mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(callback));

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&ParseCRLSet, std::move(crl_set)),
      base::BindOnce(&CertVerifierServiceFactoryImpl::OnCRLSetParsed,
                     weak_factory_.GetWeakPtr())
          .Then(std::move(callback)));
}

#if BUILDFLAG(IS_CT_SUPPORTED)
void CertVerifierServiceFactoryImpl::UpdateCtLogList(
    std::vector<network::mojom::CTLogInfoPtr> log_list,
    base::Time update_time,
    UpdateCtLogListCallback callback) {
  std::vector<scoped_refptr<const net::CTLogVerifier>> ct_logs;
  for (auto& log : log_list) {
    scoped_refptr<const net::CTLogVerifier> log_verifier =
        net::CTLogVerifier::Create(log->public_key, log->name);
    if (!log_verifier) {
      // TODO(crbug.com/40767441): Signal bad configuration (such as bad key).
      continue;
    }
    ct_logs.push_back(std::move(log_verifier));
  }

  proc_params_.ct_logs = std::move(ct_logs);

  std::vector<std::pair<std::string, base::Time>> disqualified_logs;
  std::map<std::string, certificate_transparency::LogInfo> log_info;
  ComputeCTLogInfo(log_list, &disqualified_logs, &log_info);

  proc_params_.ct_policy_enforcer =
      base::MakeRefCounted<certificate_transparency::ChromeCTPolicyEnforcer>(
          update_time, std::move(disqualified_logs), std::move(log_info),
          base::FeatureList::IsEnabled(
              net::features::kEnableStaticCTAPIEnforcement));

  UpdateVerifierServices();

  std::move(callback).Run();
}
#endif

void CertVerifierServiceFactoryImpl::OnCRLSetParsed(
    scoped_refptr<net::CRLSet> parsed_crl_set) {
  if (!parsed_crl_set) {
    return;
  }

  if (proc_params_.crl_set->sequence() >= parsed_crl_set->sequence()) {
    // Don't allow downgrades, and don't refresh CRLSets that are identical
    // (the sequence is globally unique for all CRLSets).
    return;
  }

  proc_params_.crl_set = std::move(parsed_crl_set);

  UpdateVerifierServices();
}

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
void CertVerifierServiceFactoryImpl::UpdateChromeRootStore(
    mojo_base::ProtoWrapper new_root_store,
    UpdateChromeRootStoreCallback callback) {
  // Ensure the callback is run regardless which return path is used.
  base::ScopedClosureRunner scoped_callback_runner(std::move(callback));

  auto message = new_root_store.As<chrome_root_store::RootStore>();
  if (!message.has_value()) {
    LOG(ERROR) << "error parsing proto for Chrome Root Store";
    return;
  }

  // We only check against the compiled version to allow for us to to use
  // Component Updater to revert to older versions. Check is left in
  // to guard against Component updater being stuck on older versions due
  // to daily updates of the PKI Metadata component being broken.
  if (message->version_major() <= net::CompiledChromeRootStoreVersion()) {
    return;
  }

  std::optional<net::ChromeRootStoreData> root_store_data =
      net::ChromeRootStoreData::CreateChromeRootStoreData(message.value());
  if (!root_store_data) {
    LOG(ERROR) << "error interpreting proto for Chrome Root Store";
    return;
  }

  if (root_store_data->anchors().empty()) {
    LOG(ERROR) << "parsed root store contained no anchors";
    return;
  }

  // Update the stored Chrome Root Store so that new CertVerifierService
  // instances will start with the updated store.
  proc_params_.root_store_data = std::move(root_store_data);

  UpdateVerifierServices();
}

void CertVerifierServiceFactoryImpl::GetChromeRootStoreInfo(
    GetChromeRootStoreInfoCallback callback) {
  mojom::ChromeRootStoreInfoPtr info_ptr = mojom::ChromeRootStoreInfo::New();

  std::vector<net::ChromeRootStoreData::Anchor> anchors;
  if (proc_params_.root_store_data) {
    info_ptr->version = proc_params_.root_store_data->version();
    anchors = proc_params_.root_store_data->anchors();
  } else {
    info_ptr->version = net::CompiledChromeRootStoreVersion();
    anchors = net::CompiledChromeRootStoreAnchors();
  }

  for (const auto& anchor : anchors) {
    if (!IsAnchorTrustedOnThisChromeVersion(anchor)) {
      continue;
    }
    const bssl::ParsedCertificate* cert = anchor.certificate.get();
    base::span<const uint8_t> cert_bytes =
        net::x509_util::CryptoBufferAsSpan(cert->cert_buffer());
    info_ptr->root_cert_info.push_back(mojom::ChromeRootCertInfo::New(
        GetHash(*cert),
        std::vector<uint8_t>(cert_bytes.begin(), cert_bytes.end())));
  }

  std::move(callback).Run(std::move(info_ptr));
}

// TODO(crbug.com/40928765): look into adding a test here. Possible ways to do
// this:
//  * add a SetSystemTrustStoreForTesting() call, have code use that if its set.
//  * save a SystemTrustStore upon first call add a
//  UpdateSystemTrustStoreForTesting
void CertVerifierServiceFactoryImpl::GetPlatformRootStoreInfo(
    GetPlatformRootStoreInfoCallback callback) {
  mojom::PlatformRootStoreInfoPtr info_ptr =
      mojom::PlatformRootStoreInfo::New();
  std::unique_ptr<net::SystemTrustStore> system_trust_store =
      net::CreateSslSystemTrustStoreChromeRoot(
          std::make_unique<net::TrustStoreChrome>());

  net::PlatformTrustStore* platform_trust_store =
      system_trust_store->GetPlatformTrustStore();
  if (!platform_trust_store) {
    std::move(callback).Run(std::move(info_ptr));
    return;
  }

  for (const auto& cert_with_trust :
       platform_trust_store->GetAllUserAddedCerts()) {
    mojom::CertificateTrust mojo_trust;
    switch (cert_with_trust.trust.type) {
      case bssl::CertificateTrustType::DISTRUSTED:
        mojo_trust = mojom::CertificateTrust::kDistrusted;
        break;
      case bssl::CertificateTrustType::UNSPECIFIED:
        mojo_trust = mojom::CertificateTrust::kUnspecified;
        break;
      case bssl::CertificateTrustType::TRUSTED_ANCHOR:
      case bssl::CertificateTrustType::TRUSTED_LEAF:
      case bssl::CertificateTrustType::TRUSTED_ANCHOR_OR_LEAF:
        mojo_trust = mojom::CertificateTrust::kTrusted;
    }
    info_ptr->user_added_certs.push_back(
        mojom::PlatformCertInfo::New(cert_with_trust.cert_bytes, mojo_trust));
  }

  std::move(callback).Run(std::move(info_ptr));
}
#endif

void CertVerifierServiceFactoryImpl::UpdateNetworkTime(
    base::Time system_time,
    base::TimeTicks system_ticks,
    base::Time current_time) {
  proc_params_.time_tracker.emplace(system_time, system_ticks, current_time,
                                    base::TimeDelta());
  UpdateVerifierServices();
}

#if BUILDFLAG(CHROME_ROOT_STORE_OPTIONAL)
void CertVerifierServiceFactoryImpl::SetUseChromeRootStore(
    bool use_crs,
    SetUseChromeRootStoreCallback callback) {
  if (use_crs != proc_params_.use_chrome_root_store) {
    proc_params_.use_chrome_root_store = use_crs;
    UpdateVerifierServices();
  }
  std::move(callback).Run();
}
#endif

void CertVerifierServiceFactoryImpl::RemoveService(
    internal::CertVerifierServiceImpl* service_impl) {
  verifier_services_.erase(service_impl);
}

void CertVerifierServiceFactoryImpl::UpdateVerifierServices() {
  for (internal::CertVerifierServiceImpl* service : verifier_services_) {
    service->UpdateVerifierData(proc_params_);
  }
}

base::WeakPtr<CertVerifierServiceFactoryImpl>
CertVerifierServiceFactoryImpl::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace cert_verifier
