// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/blob_storage/blob_registry_wrapper.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_features.h"
#include "net/base/features.h"
#include "storage/browser/blob/blob_registry_impl.h"
#include "storage/browser/blob/blob_storage_context.h"

namespace content {

namespace {

class BindingDelegate : public storage::BlobRegistryImpl::Delegate {
 public:
  explicit BindingDelegate(
      ChildProcessSecurityPolicyImpl::Handle security_policy_handle)
      : security_policy_handle_(std::move(security_policy_handle)) {}
  ~BindingDelegate() override {}

  bool CanReadFile(const base::FilePath& file) override {
    return security_policy_handle_.CanReadFile(file);
  }
  bool CanAccessDataForOrigin(const url::Origin& origin) override {
    return security_policy_handle_.CanAccessDataForOrigin(origin);
  }
  file_access::ScopedFileAccessDelegate::RequestFilesAccessIOCallback
  GetAccessCallback() override {
    // TODO (b/262203074) create actual callback
    return base::NullCallback();
  }

 private:
  ChildProcessSecurityPolicyImpl::Handle security_policy_handle_;
};

}  // namespace

// static
scoped_refptr<BlobRegistryWrapper> BlobRegistryWrapper::Create(
    scoped_refptr<ChromeBlobStorageContext> blob_storage_context,
    base::WeakPtr<storage::BlobUrlRegistry> blob_url_registry) {
  DCHECK(
      !base::FeatureList::IsEnabled(net::features::kSupportPartitionedBlobUrl));
  scoped_refptr<BlobRegistryWrapper> result(new BlobRegistryWrapper());
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&BlobRegistryWrapper::InitializeOnIOThreadDeprecated,
                     result, std::move(blob_storage_context),
                     std::move(blob_url_registry)));
  return result;
}

// static
scoped_refptr<BlobRegistryWrapper> BlobRegistryWrapper::Create(
    scoped_refptr<ChromeBlobStorageContext> blob_storage_context) {
  DCHECK(
      base::FeatureList::IsEnabled(net::features::kSupportPartitionedBlobUrl));
  scoped_refptr<BlobRegistryWrapper> result(new BlobRegistryWrapper());
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&BlobRegistryWrapper::InitializeOnIOThread,
                                result, std::move(blob_storage_context)));
  return result;
}

BlobRegistryWrapper::BlobRegistryWrapper() {
}

void BlobRegistryWrapper::Bind(
    int process_id,
    mojo::PendingReceiver<blink::mojom::BlobRegistry> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  blob_registry_->Bind(
      std::move(receiver),
      std::make_unique<BindingDelegate>(
          ChildProcessSecurityPolicyImpl::GetInstance()->CreateHandle(
              process_id)));
}

BlobRegistryWrapper::~BlobRegistryWrapper() {}

void BlobRegistryWrapper::InitializeOnIOThreadDeprecated(
    scoped_refptr<ChromeBlobStorageContext> blob_storage_context,
    base::WeakPtr<storage::BlobUrlRegistry> blob_url_registry) {
  DCHECK(
      !base::FeatureList::IsEnabled(net::features::kSupportPartitionedBlobUrl));
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  blob_registry_ = std::make_unique<storage::BlobRegistryImpl>(
      blob_storage_context->context()->AsWeakPtr(),
      std::move(blob_url_registry), GetUIThreadTaskRunner({}));
}

void BlobRegistryWrapper::InitializeOnIOThread(
    scoped_refptr<ChromeBlobStorageContext> blob_storage_context) {
  DCHECK(
      base::FeatureList::IsEnabled(net::features::kSupportPartitionedBlobUrl));
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  blob_registry_ = std::make_unique<storage::BlobRegistryImpl>(
      blob_storage_context->context()->AsWeakPtr());
}

}  // namespace content
