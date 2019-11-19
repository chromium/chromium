// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_url_store_impl.h"

#include "base/bind.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "storage/browser/blob/blob_impl.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/blob/blob_url_loader_factory.h"
#include "storage/browser/blob/blob_url_utils.h"

namespace storage {

// Self deletes when the last binding to it is closed.
class BlobURLTokenImpl : public blink::mojom::BlobURLToken {
 public:
  BlobURLTokenImpl(base::WeakPtr<BlobStorageContext> context,
                   const GURL& url,
                   mojo::PendingRemote<blink::mojom::Blob> blob,
                   mojo::PendingReceiver<blink::mojom::BlobURLToken> receiver)
      : context_(std::move(context)),
        url_(url),
        token_(base::UnguessableToken::Create()) {
    receivers_.Add(this, std::move(receiver));
    receivers_.set_disconnect_handler(base::BindRepeating(
        &BlobURLTokenImpl::OnConnectionError, base::Unretained(this)));
    if (context_) {
      context_->mutable_registry()->AddTokenMapping(token_, url_,
                                                    std::move(blob));
    }
  }

  ~BlobURLTokenImpl() override {
    if (context_)
      context_->mutable_registry()->RemoveTokenMapping(token_);
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

  base::WeakPtr<BlobStorageContext> context_;
  mojo::ReceiverSet<blink::mojom::BlobURLToken> receivers_;
  const GURL url_;
  const base::UnguessableToken token_;
};

BlobURLStoreImpl::BlobURLStoreImpl(base::WeakPtr<BlobStorageContext> context,
                                   BlobRegistryImpl::Delegate* delegate)
    : context_(std::move(context)), delegate_(delegate) {}

BlobURLStoreImpl::~BlobURLStoreImpl() {
  if (context_) {
    for (const auto& url : urls_)
      context_->RevokePublicBlobURL(url);
  }
}

void BlobURLStoreImpl::Register(mojo::PendingRemote<blink::mojom::Blob> blob,
                                const GURL& url,
                                RegisterCallback callback) {
  if (!url.SchemeIsBlob()) {
    mojo::ReportBadMessage("Invalid scheme passed to BlobURLStore::Register");
    std::move(callback).Run();
    return;
  }
  // Only report errors when we don't have permission to commit and
  // the process is valid. The process check is a temporary solution to
  // handle cases where this method is run after the
  // process associated with |delegate_| has been destroyed.
  // See https://crbug.com/933089 for details.
  if (!delegate_->CanCommitURL(url) && delegate_->IsProcessValid()) {
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

  if (context_)
    context_->RegisterPublicBlobURL(url, std::move(blob));
  urls_.insert(url);
  std::move(callback).Run();
}

void BlobURLStoreImpl::Revoke(const GURL& url) {
  if (!url.SchemeIsBlob()) {
    mojo::ReportBadMessage("Invalid scheme passed to BlobURLStore::Revoke");
    return;
  }
  // Only report errors when we don't have permission to commit and
  // the process is valid. The process check is a temporary solution to
  // handle cases where this method is run after the
  // process associated with |delegate_| has been destroyed.
  // See https://crbug.com/933089 for details.
  if (!delegate_->CanCommitURL(url) && delegate_->IsProcessValid()) {
    mojo::ReportBadMessage(
        "Non committable URL passed to BlobURLStore::Revoke");
    return;
  }
  if (BlobUrlUtils::UrlHasFragment(url)) {
    mojo::ReportBadMessage("URL with fragment passed to BlobURLStore::Revoke");
    return;
  }

  if (context_)
    context_->RevokePublicBlobURL(url);
  urls_.erase(url);
}

void BlobURLStoreImpl::Resolve(const GURL& url, ResolveCallback callback) {
  if (!context_) {
    std::move(callback).Run(mojo::NullRemote());
    return;
  }
  mojo::PendingRemote<blink::mojom::Blob> blob =
      context_->GetBlobFromPublicURL(url);
  std::move(callback).Run(std::move(blob));
}

void BlobURLStoreImpl::ResolveAsURLLoaderFactory(
    const GURL& url,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver) {
  BlobURLLoaderFactory::Create(
      context_ ? context_->GetBlobFromPublicURL(url) : mojo::NullRemote(), url,
      context_, std::move(receiver));
}

void BlobURLStoreImpl::ResolveForNavigation(
    const GURL& url,
    mojo::PendingReceiver<blink::mojom::BlobURLToken> token) {
  if (!context_)
    return;
  mojo::PendingRemote<blink::mojom::Blob> blob =
      context_->GetBlobFromPublicURL(url);
  if (!blob)
    return;
  new BlobURLTokenImpl(context_, url, std::move(blob), std::move(token));
}

}  // namespace storage
