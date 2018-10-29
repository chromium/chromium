// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/blob_storage/blob_registry_wrapper.h"

#include "base/task/post_task.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/common/content_features.h"
#include "storage/browser/blob/blob_registry_impl.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/fileapi/file_system_context.h"

namespace content {

namespace {

class BindingDelegate : public storage::BlobRegistryImpl::Delegate {
 public:
  explicit BindingDelegate(int process_id) : process_id_(process_id) {}
  ~BindingDelegate() override {}

  bool CanReadFile(const base::FilePath& file) override {
    ChildProcessSecurityPolicyImpl* security_policy =
        ChildProcessSecurityPolicyImpl::GetInstance();
    return security_policy->CanReadFile(process_id_, file);
  }
  bool CanReadFileSystemFile(const storage::FileSystemURL& url) override {
    ChildProcessSecurityPolicyImpl* security_policy =
        ChildProcessSecurityPolicyImpl::GetInstance();
    return security_policy->CanReadFileSystemFile(process_id_, url);
  }
  bool CanCommitURL(const GURL& url) override {
    ChildProcessSecurityPolicyImpl* security_policy =
        ChildProcessSecurityPolicyImpl::GetInstance();
    return security_policy->CanCommitURL(process_id_, url);
  }

 private:
  const int process_id_;
};

}  // namespace

// static
scoped_refptr<BlobRegistryWrapper> BlobRegistryWrapper::Create(
    scoped_refptr<ChromeBlobStorageContext> blob_storage_context,
    scoped_refptr<storage::FileSystemContext> file_system_context) {
  scoped_refptr<BlobRegistryWrapper> result(new BlobRegistryWrapper());
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&BlobRegistryWrapper::InitializeOnIOThread, result,
                     std::move(blob_storage_context),
                     std::move(file_system_context)));
  return result;
}

BlobRegistryWrapper::BlobRegistryWrapper() {
}

void BlobRegistryWrapper::Bind(int process_id,
                               blink::mojom::BlobRegistryRequest request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  blob_registry_->Bind(std::move(request),
                       std::make_unique<BindingDelegate>(process_id));
}

BlobRegistryWrapper::~BlobRegistryWrapper() {}

void BlobRegistryWrapper::InitializeOnIOThread(
    scoped_refptr<ChromeBlobStorageContext> blob_storage_context,
    scoped_refptr<storage::FileSystemContext> file_system_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  blob_registry_ = std::make_unique<storage::BlobRegistryImpl>(
      blob_storage_context->context()->AsWeakPtr(),
      std::move(file_system_context));
}

}  // namespace content
