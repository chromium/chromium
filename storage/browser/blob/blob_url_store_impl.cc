// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_url_store_impl.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/safety_checks.h"
#include "base/strings/strcat.h"
#include "components/crash/core/common/crash_key.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "net/base/features.h"
#include "storage/browser/blob/blob_impl.h"
#include "storage/browser/blob/blob_url_loader_factory.h"
#include "storage/browser/blob/blob_url_registry.h"
#include "storage/browser/blob/blob_url_utils.h"
#include "storage/browser/blob/features.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom.h"
#include "url/url_util.h"

namespace storage {
namespace {

bool IsBlobUrlAccessCrossPartitionSameOrigin(
    BlobUrlRegistry::MappingStatus mapping_status) {
  return mapping_status ==
             BlobUrlRegistry::MappingStatus::
                 kNotMappedCrossPartitionSameOriginAccessFirstPartyBlobURL ||
         mapping_status ==
             BlobUrlRegistry::MappingStatus::
                 kNotMappedCrossPartitionSameOriginAccessThirdPartyBlobURL;
}
}  // namespace

// Self deletes when the last binding to it is closed.
class BlobURLTokenImpl : public blink::mojom::BlobURLToken {
 public:
  BlobURLTokenImpl(base::WeakPtr<BlobUrlRegistry> registry,
                   const GURL& url,
                   mojo::PendingRemote<blink::mojom::Blob> blob,
                   mojo::PendingReceiver<blink::mojom::BlobURLToken> receiver)
      : registry_(std::move(registry)),
        url_(url),
        token_(base::UnguessableToken::Create()) {
    receivers_.Add(this, std::move(receiver));
    receivers_.set_disconnect_handler(base::BindRepeating(
        &BlobURLTokenImpl::OnConnectionError, base::Unretained(this)));
    if (registry_) {
      registry_->AddTokenMapping(token_, url_, std::move(blob));
    }
  }

  ~BlobURLTokenImpl() override {
    if (registry_)
      registry_->RemoveTokenMapping(token_);
  }

  void GetToken(GetTokenCallback callback) override {
    std::move(callback).Run(token_);
  }

  void Clone(
      mojo::PendingReceiver<blink::mojom::BlobURLToken> receiver) override {
    receivers_.Add(this, std::move(receiver));
  }

 private:
  void OnConnectionError() {
    if (!receivers_.empty())
      return;
    delete this;
  }

  base::WeakPtr<BlobUrlRegistry> registry_;
  mojo::ReceiverSet<blink::mojom::BlobURLToken> receivers_;
  const GURL url_;
  const base::UnguessableToken token_;
};

BlobURLStoreImpl::BlobURLStoreImpl(
    const blink::StorageKey& storage_key,
    const url::Origin& renderer_origin,
    int render_process_host_id,
    base::WeakPtr<BlobUrlRegistry> registry,
    BlobURLValidityCheckBehavior validity_check_behavior,
    base::RepeatingCallback<
        void(const GURL&, std::optional<blink::mojom::PartitioningBlobURLInfo>)>
        partitioning_blob_url_closure,
    base::RepeatingCallback<bool()> storage_access_check_callback,
    bool partitioning_disabled_by_policy)
    : storage_key_(storage_key),
      renderer_origin_(renderer_origin),
      render_process_host_id_(render_process_host_id),
      registry_(std::move(registry)),
      validity_check_behavior_(validity_check_behavior),
      partitioning_blob_url_closure_(std::move(partitioning_blob_url_closure)),
      storage_access_check_callback_(std::move(storage_access_check_callback)),
      partitioning_disabled_by_policy_(partitioning_disabled_by_policy) {}

BlobURLStoreImpl::~BlobURLStoreImpl() {
  if (registry_) {
    for (const auto& url : urls_)
      registry_->RemoveUrlMapping(url, storage_key_);
  }
}

void BlobURLStoreImpl::Register(
    mojo::PendingRemote<blink::mojom::Blob> blob,
    const GURL& url,
    // TODO(crbug.com/40775506): Remove these once experiment is over.
    const base::UnguessableToken& unsafe_agent_cluster_id,
    const std::optional<net::SchemefulSite>& unsafe_top_level_site,
    RegisterCallback callback) {
  // TODO(crbug.com/40061399): Generate blob URLs here, rather than
  // validating the URLs the renderer process generated.
  if (!BlobUrlIsValid(url, "Register")) {
    std::move(callback).Run();
    return;
  }

  if (registry_)
    registry_->AddUrlMapping(url, std::move(blob), storage_key_,
                             renderer_origin_, render_process_host_id_,
                             unsafe_agent_cluster_id, unsafe_top_level_site);
  urls_.insert(url);
  std::move(callback).Run();
}

void BlobURLStoreImpl::Revoke(const GURL& url) {
  if (!BlobUrlIsValid(url, "Revoke"))
    return;

  if (registry_)
    registry_->RemoveUrlMapping(url, storage_key_);
  urls_.erase(url);
}

bool BlobURLStoreImpl::ShouldPartitionBlobUrlAccess(
    bool has_storage_access_handle,
    BlobUrlRegistry::MappingStatus mapping_status) {
  const bool feature_and_policy_check =
      base::FeatureList::IsEnabled(
          features::kBlockCrossPartitionBlobUrlFetching) &&
      !partitioning_disabled_by_policy_;

  const bool should_bypass_partitioning =
      has_storage_access_handle &&
      mapping_status ==
          BlobUrlRegistry::MappingStatus::
              kNotMappedCrossPartitionSameOriginAccessFirstPartyBlobURL;
  return feature_and_policy_check && !should_bypass_partitioning;
}

void BlobURLStoreImpl::ResolveAsURLLoaderFactory(
    const GURL& url,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
    ResolveAsURLLoaderFactoryCallback callback) {
  if (!registry_) {
    BlobURLLoaderFactory::Create(mojo::NullRemote(), url, std::move(receiver));
    std::move(callback).Run(std::nullopt, std::nullopt);
    return;
  }
  FinishResolveAsURLLoaderFactory(url, std::move(receiver), std::move(callback),
                                  storage_access_check_callback_.Run());
}

void BlobURLStoreImpl::FinishResolveAsURLLoaderFactory(
    const GURL& url,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
    ResolveAsURLLoaderFactoryCallback callback,
    bool has_storage_access_handle) {
  const BlobUrlRegistry::MappingStatus mapping_status =
      registry_->IsUrlMapped(BlobUrlUtils::ClearUrlFragment(url), storage_key_);
  if (IsBlobUrlAccessCrossPartitionSameOrigin(mapping_status)) {
    if (ShouldPartitionBlobUrlAccess(has_storage_access_handle,
                                     mapping_status)) {
      partitioning_blob_url_closure_.Run(url,
                                         blink::mojom::PartitioningBlobURLInfo::
                                             kBlockedCrossPartitionFetching);
      BlobURLLoaderFactory::Create(mojo::NullRemote(), url,
                                   std::move(receiver));
      std::move(callback).Run(std::nullopt, std::nullopt);
      return;
    }
    partitioning_blob_url_closure_.Run(url, std::nullopt);
  }

  BlobURLLoaderFactory::Create(registry_->GetBlobFromUrl(url), url,
                               std::move(receiver));
  // When a fragment URL is present, registry_->GetUnsafeAgentClusterID(url) and
  // registry_->GetUnsafeTopLevelSite(url) will return nullopt because their
  // implementations don't remove the fragment and only support fragmentless
  // URLs (crbug.com/40775506).
  std::move(callback).Run(registry_->GetUnsafeAgentClusterID(url),
                          registry_->GetUnsafeTopLevelSite(url));
}

void BlobURLStoreImpl::ResolveAsBlobURLToken(
    const GURL& url,
    mojo::PendingReceiver<blink::mojom::BlobURLToken> token,
    bool is_top_level_navigation,
    ResolveAsBlobURLTokenCallback callback) {
  // This function is known to be heap allocation heavy and performance
  // critical. Extra memory safety checks can introduce regression
  // (https://crbug.com/414710225) and these are disabled here.
  base::ScopedSafetyChecksExclusion scoped_unsafe;

  if (!registry_) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  FinishResolveAsBlobURLToken(url, std::move(token), is_top_level_navigation,
                              std::move(callback),
                              storage_access_check_callback_.Run());
}

void BlobURLStoreImpl::FinishResolveAsBlobURLToken(
    const GURL& url,
    mojo::PendingReceiver<blink::mojom::BlobURLToken> token,
    bool is_top_level_navigation,
    ResolveAsBlobURLTokenCallback callback,
    bool has_storage_access_handle) {
  if (!is_top_level_navigation) {
    const BlobUrlRegistry::MappingStatus mapping_status =
        registry_->IsUrlMapped(BlobUrlUtils::ClearUrlFragment(url),
                               storage_key_);
    if (IsBlobUrlAccessCrossPartitionSameOrigin(mapping_status)) {
      if (ShouldPartitionBlobUrlAccess(has_storage_access_handle,
                                       mapping_status)) {
        partitioning_blob_url_closure_.Run(
            url, blink::mojom::PartitioningBlobURLInfo::
                     kBlockedCrossPartitionFetching);
        std::move(callback).Run(std::nullopt);
        return;
      }
      partitioning_blob_url_closure_.Run(url, std::nullopt);
    }
  }

  mojo::PendingRemote<blink::mojom::Blob> blob = registry_->GetBlobFromUrl(url);
  if (!blob) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  new BlobURLTokenImpl(registry_, url, std::move(blob), std::move(token));
  std::move(callback).Run(registry_->GetUnsafeAgentClusterID(url));
}

bool BlobURLStoreImpl::BlobUrlIsValid(const GURL& url,
                                      const char* method) const {
  // TODO(crbug.com/40810120): Remove crash keys.
  url::Origin storage_key_origin = storage_key_.origin();
  static crash_reporter::CrashKeyString<256> origin_key("origin");
  static crash_reporter::CrashKeyString<256> url_key("url");
  crash_reporter::ScopedCrashKeyString scoped_origin_key(
      &origin_key, storage_key_origin.GetDebugString());
  crash_reporter::ScopedCrashKeyString scoped_url_key(
      &url_key, url.possibly_invalid_spec());

  if (!url.SchemeIsBlob()) {
    mojo::ReportBadMessage(
        base::StrCat({"Invalid scheme passed to BlobURLStore::", method}));
    return false;
  }
  url::Origin url_origin = url::Origin::Create(url);
  // For file:// origins blink sometimes creates blob URLs with "null" as origin
  // and other times "file://" (based on a runtime setting). On the other hand,
  // `origin_` will always be a non-opaque file: origin for pages loaded from
  // file:// URLs. To deal with this, we treat file:// origins and
  // opaque origins separately from non-opaque origins.
  // URLs created by blink::BlobURL::CreateBlobURL() will always get "blank" as
  // origin if the scheme is local, which usually includes the file scheme and
  // on Android also the content scheme.
  bool valid_origin = true;
  if (url_origin.scheme() == url::kFileScheme) {
    valid_origin = storage_key_origin.scheme() == url::kFileScheme;
  } else if (url_origin.opaque()) {
    // TODO(crbug.com/40051700): Once `storage_key_` corresponds to an
    // opaque origin under the circumstances described in the crbug, remove the
    // ALLOW_OPAQUE_ORIGIN_STORAGE_KEY_MISMATCH workaround here.
    valid_origin =
        storage_key_origin.opaque() ||
        base::Contains(url::GetLocalSchemes(), storage_key_origin.scheme()) ||
        validity_check_behavior_ ==
            BlobURLValidityCheckBehavior::
                ALLOW_OPAQUE_ORIGIN_STORAGE_KEY_MISMATCH;
  } else {
    valid_origin = storage_key_origin == url_origin;
  }
  if (!valid_origin) {
    mojo::ReportBadMessage(base::StrCat(
        {"URL with invalid origin passed to BlobURLStore::", method}));
    return false;
  }
  if (BlobUrlUtils::UrlHasFragment(url)) {
    mojo::ReportBadMessage(
        base::StrCat({"URL with fragment passed to BlobURLStore::", method}));
    return false;
  }
  return true;
}

}  // namespace storage
