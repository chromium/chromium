// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/blob_storage/blob_registry_wrapper.h"

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/process_lock.h"
#include "content/browser/security/cpsp/child_process_security_policy_impl.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/blob/blob_registry_impl.h"
#include "storage/browser/blob/blob_storage_context.h"

namespace content {

namespace {

class BindingDelegate : public storage::BlobRegistryImpl::Delegate {
 public:
  explicit BindingDelegate(
      ChildProcessSecurityPolicyImpl::Handle security_policy_handle)
      : security_policy_handle_(std::move(security_policy_handle)) {}
  ~BindingDelegate() override = default;

  bool CanReadFile(const base::FilePath& file) override {
    return security_policy_handle_.CanReadFile(file);
  }
  bool CanAccessDataForOrigin(const url::Origin& origin) override {
    return security_policy_handle_.CanAccessDataForOrigin(origin);
  }
  // Returns an interned RefCountedString identifying the site/origin of the
  // renderer process creating the blob. Deduplicates origin strings across all
  // blobs and stream chunks created by this renderer process.
  scoped_refptr<base::RefCountedString> GetCreatorIdentity() override {
    if (cached_identity_) {
      return cached_identity_;
    }
    ProcessLock lock =
        ChildProcessSecurityPolicyImpl::GetInstance()->GetProcessLock(
            security_policy_handle_.child_id());
    if (lock.IsLockedToSite()) {
      cached_identity_ = base::MakeRefCounted<base::RefCountedString>(
          lock.GetProcessLockURL().spec());
      return cached_identity_;
    }
    // Do not cache in `cached_identity_` below because an unassociated or
    // unlocked process may later become locked to a site upon navigation
    // commit.
    if (lock.AllowsAnySite()) {
      static base::NoDestructor<scoped_refptr<base::RefCountedString>>
          kUnlockedIdentity(base::MakeRefCounted<base::RefCountedString>(
              "Unlocked Renderer"));
      return *kUnlockedIdentity;
    }
    CHECK(lock.is_invalid(), base::NotFatalUntil::M160);
    static base::NoDestructor<scoped_refptr<base::RefCountedString>>
        kInvalidLockIdentity(base::MakeRefCounted<base::RefCountedString>(
            "Invalid ProcessLock"));
    return *kInvalidLockIdentity;
  }

 private:
  ChildProcessSecurityPolicyImpl::Handle security_policy_handle_;
  scoped_refptr<base::RefCountedString> cached_identity_;
};

}  // namespace

// static
scoped_refptr<BlobRegistryWrapper> BlobRegistryWrapper::Create(
    scoped_refptr<ChromeBlobStorageContext> blob_storage_context) {
  scoped_refptr<BlobRegistryWrapper> result(new BlobRegistryWrapper());
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&BlobRegistryWrapper::InitializeOnIOThread,
                                result, std::move(blob_storage_context)));
  return result;
}

BlobRegistryWrapper::BlobRegistryWrapper() = default;

void BlobRegistryWrapper::Bind(
    ChildProcessId process_id,
    mojo::PendingReceiver<blink::mojom::BlobRegistry> receiver) {
  CHECK_CURRENTLY_ON(BrowserThread::IO, base::NotFatalUntil::M159);
  blob_registry_->Bind(
      std::move(receiver),
      std::make_unique<BindingDelegate>(
          ChildProcessSecurityPolicyImpl::GetInstance()->CreateHandle(
              process_id)));
}

BlobRegistryWrapper::~BlobRegistryWrapper() = default;

void BlobRegistryWrapper::InitializeOnIOThread(
    scoped_refptr<ChromeBlobStorageContext> blob_storage_context) {
  CHECK_CURRENTLY_ON(BrowserThread::IO, base::NotFatalUntil::M159);
  blob_registry_ = std::make_unique<storage::BlobRegistryImpl>(
      blob_storage_context->context()->AsWeakPtr());
}

}  // namespace content
