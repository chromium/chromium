// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_url_store_impl.h"

#include "base/bind.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "storage/browser/blob/blob_impl.h"
#include "storage/browser/blob/blob_url_loader_factory.h"
#include "storage/browser/blob/blob_url_registry.h"
#include "storage/browser/blob/blob_url_utils.h"

namespace storage {

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

BlobURLStoreImpl::BlobURLStoreImpl(base::WeakPtr<BlobUrlRegistry> registry,
                                   BlobRegistryImpl::Delegate* delegate)
    : registry_(std::move(registry)), delegate_(delegate) {}

BlobURLStoreImpl::~BlobURLStoreImpl() {
  if (registry_) {
    for (const auto& url : urls_)
      registry_->RemoveUrlMapping(url);
  }
}

void BlobURLStoreImpl::Register(
    mojo::PendingRemote<blink::mojom::Blob> blob,
    const GURL& url,
    // TODO(https://crbug.com/1224926): Remove this once experiment is over.
    const base::UnguessableToken& unsafe_agent_cluster_id,
    RegisterCallback callback) {
  if (!url.SchemeIsBlob()) {
    mojo::ReportBadMessage("Invalid scheme passed to BlobURLStore::Register");
    std::move(callback).Run();
    return;
  }
  if (!delegate_->CanCommitURL(url)) {
    mojo::ReportBadMessage(
        "Non committable URL passed to BlobURLStore::Register");
    std::move(callback).Run();
    return;
  }
  if (BlobUrlUtils::UrlHasFragment(url)) {
    mojo::ReportBadMessage(
        "URL with fragment passed to BlobURLStore::Register");
    std::move(callback).Run();
    return;
  }

  if (registry_)
    registry_->AddUrlMapping(url, std::move(blob), unsafe_agent_cluster_id);
  urls_.insert(url);
  std::move(callback).Run();
}

void BlobURLStoreImpl::Revoke(const GURL& url) {
  if (!url.SchemeIsBlob()) {
    mojo::ReportBadMessage("Invalid scheme passed to BlobURLStore::Revoke");
    return;
  }
  if (!delegate_->CanCommitURL(url)) {
    mojo::ReportBadMessage(
        "Non committable URL passed to BlobURLStore::Revoke");
    return;
  }
  if (BlobUrlUtils::UrlHasFragment(url)) {
    mojo::ReportBadMessage("URL with fragment passed to BlobURLStore::Revoke");
    return;
  }

  if (registry_)
    registry_->RemoveUrlMapping(url);
  urls_.erase(url);
}

void BlobURLStoreImpl::Resolve(const GURL& url, ResolveCallback callback) {
  if (!registry_) {
    std::move(callback).Run(mojo::NullRemote(), absl::nullopt);
    return;
  }
  mojo::PendingRemote<blink::mojom::Blob> blob = registry_->GetBlobFromUrl(url);
  std::move(callback).Run(std::move(blob),
                          registry_->GetUnsafeAgentClusterID(url));
}

void BlobURLStoreImpl::ResolveAsURLLoaderFactory(
    const GURL& url,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
    ResolveAsURLLoaderFactoryCallback callback) {
  BlobURLLoaderFactory::Create(
      registry_ ? registry_->GetBlobFromUrl(url) : mojo::NullRemote(), url,
      std::move(receiver));
  std::move(callback).Run(registry_->GetUnsafeAgentClusterID(url));
}

void BlobURLStoreImpl::ResolveForNavigation(
    const GURL& url,
    mojo::PendingReceiver<blink::mojom::BlobURLToken> token,
    ResolveForNavigationCallback callback) {
  if (!registry_) {
    std::move(callback).Run(absl::nullopt);
    return;
  }
  mojo::PendingRemote<blink::mojom::Blob> blob = registry_->GetBlobFromUrl(url);
  if (!blob) {
    std::move(callback).Run(absl::nullopt);
    return;
  }
  new BlobURLTokenImpl(registry_, url, std::move(blob), std::move(token));
  std::move(callback).Run(registry_->GetUnsafeAgentClusterID(url));
}

}  // namespace storage
