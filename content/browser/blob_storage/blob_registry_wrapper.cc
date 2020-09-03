// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/blob_storage/blob_registry_wrapper.h"

#include "base/bind.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/common/content_features.h"
#include "storage/browser/blob/blob_registry_impl.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/file_system/file_system_context.h"

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
  bool CanReadFileSystemFile(const storage::FileSystemURL& url) override {
    return security_policy_handle_.CanReadFileSystemFile(url);
  }
  bool CanCommitURL(const GURL& url) override {
    return security_policy_handle_.CanCommitURL(url);
  }
 private:
  ChildProcessSecurityPolicyImpl::Handle security_policy_handle_;
};

}  // namespace

// static
scoped_refptr<BlobRegistryWrapper> BlobRegistryWrapper::Create(
    scoped_refptr<ChromeBlobStorageContext> blob_storage_context,
    scoped_refptr<storage::FileSystemContext> file_system_context,
    scoped_refptr<BlobRegistryWrapper> registry_for_fallback_url_registry) {
  scoped_refptr<BlobRegistryWrapper> result(new BlobRegistryWrapper());
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&BlobRegistryWrapper::InitializeOnIOThread,
                                result, std::move(blob_storage_context),
                                std::move(file_system_context),
                                std::move(registry_for_fallback_url_registry)));
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

void BlobRegistryWrapper::InitializeOnIOThread(
    scoped_refptr<ChromeBlobStorageContext> blob_storage_context,
    scoped_refptr<storage::FileSystemContext> file_system_context,
    scoped_refptr<BlobRegistryWrapper> registry_for_fallback_url_registry) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  url_registry_ = std::make_unique<storage::BlobUrlRegistry>(
      registry_for_fallback_url_registry
          ? registry_for_fallback_url_registry->url_registry()->AsWeakPtr()
          : nullptr);
  blob_registry_ = std::make_unique<storage::BlobRegistryImpl>(
      blob_storage_context->context()->AsWeakPtr(), url_registry_->AsWeakPtr(),
      std::move(file_system_context));
}

}  // namespace content
