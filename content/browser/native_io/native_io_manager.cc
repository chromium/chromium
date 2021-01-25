// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/native_io/native_io_manager.h"

#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "content/browser/native_io/native_io_host.h"
#include "content/browser/native_io/native_io_quota_client.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "storage/browser/quota/quota_client.h"
#include "storage/browser/quota/quota_client_type.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "storage/common/database/database_identifier.h"
#include "third_party/blink/public/common/native_io/native_io_utils.h"
#include "third_party/blink/public/mojom/native_io/native_io.mojom.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom-shared.h"
#include "url/origin.h"

namespace content {

namespace {

constexpr base::FilePath::CharType kNativeIODirectoryName[] =
    FILE_PATH_LITERAL("NativeIO");
}  // namespace

NativeIOManager::NativeIOManager(
    const base::FilePath& profile_root,
    scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy,
    scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy)
    : root_path_(GetNativeIORootPath(profile_root)),
      special_storage_policy_(std::move(special_storage_policy)),
      quota_manager_proxy_(std::move(quota_manager_proxy)),
      // Using a raw pointer is safe since NativeIOManager be owned by
      // NativeIOQuotaClient and is guaranteed to outlive it.
      quota_client_(this),
      quota_client_receiver_(&quota_client_) {
  if (quota_manager_proxy_) {
    // Quota client assumes all backends have registered.
    quota_manager_proxy_->RegisterClient(
        quota_client_receiver_.BindNewPipeAndPassRemote(),
        storage::QuotaClientType::kNativeIO,
        {blink::mojom::StorageType::kTemporary});
  }
}

NativeIOManager::~NativeIOManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void NativeIOManager::BindReceiver(
    const url::Origin& origin,
    mojo::PendingReceiver<blink::mojom::NativeIOHost> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = hosts_.find(origin);
  if (it == hosts_.end()) {
    // This feature should only be exposed to potentially trustworthy origins
    // (https://w3c.github.io/webappsec-secure-contexts/#is-origin-trustworthy).
    // Notably this includes the https and chrome-extension schemes, among
    // others.
    if (!network::IsOriginPotentiallyTrustworthy(origin)) {
      mojo::ReportBadMessage("Called NativeIO from an insecure context");
      return;
    }

    base::FilePath origin_root_path = RootPathForOrigin(origin);
    if (origin_root_path.empty()) {
      // NativeIO is not supported for the origin.
      return;
    }

    DCHECK(root_path_.IsParent(origin_root_path))
        << "Per-origin data should be in a sub-directory of NativeIO/";

    bool insert_succeeded;
    std::tie(it, insert_succeeded) =
        hosts_.emplace(origin, std::make_unique<NativeIOHost>(
                                   this, origin, std::move(origin_root_path)));
    DCHECK(insert_succeeded);
  }

  it->second->BindReceiver(std::move(receiver));
}

void NativeIOManager::OnHostReceiverDisconnect(NativeIOHost* host) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  MaybeDeleteHost(host);
}

void NativeIOManager::MaybeDeleteHost(NativeIOHost* host) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(host != nullptr);
  DCHECK(hosts_.count(host->origin()) > 0);
  DCHECK_EQ(hosts_[host->origin()].get(), host);

  if (!host->has_empty_receiver_set() || host->delete_all_data_in_progress())
    return;

  hosts_.erase(host->origin());
}

void NativeIOManager::OnDeleteOriginDataCompleted(
    storage::QuotaClient::DeleteOriginDataCallback callback,
    base::File::Error result,
    NativeIOHost* host) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  MaybeDeleteHost(host);
  blink::mojom::QuotaStatusCode quota_result =
      result == base::File::FILE_OK ? blink::mojom::QuotaStatusCode::kOk
                                    : blink::mojom::QuotaStatusCode::kUnknown;
  std::move(callback).Run(quota_result);
}

void NativeIOManager::DeleteOriginData(
    const url::Origin& origin,
    storage::QuotaClient::DeleteOriginDataCallback callback) {
  auto it = hosts_.find(origin);
  if (it == hosts_.end()) {
    // TODO(rstz): Consider turning these checks into DCHECKS when NativeIO is
    // no longer bundled with the Filesystem API during data removal.
    if (!network::IsOriginPotentiallyTrustworthy(origin)) {
      std::move(callback).Run(blink::mojom::QuotaStatusCode::kOk);
      return;
    }
    base::FilePath origin_root_path = RootPathForOrigin(origin);
    if (origin_root_path.empty()) {
      // NativeIO is not supported for the origin, no data can be deleted.
      std::move(callback).Run(blink::mojom::QuotaStatusCode::kOk);
      return;
    }

    DCHECK(root_path_.IsParent(origin_root_path))
        << "Per-origin data should be in a sub-directory of NativeIO/";

    bool insert_succeeded;
    // Create a NativeIOHost so that future API calls for the origin are queued
    // behind the data deletion. This should not meaningfully slow down the
    // removal process.
    std::tie(it, insert_succeeded) =
        hosts_.emplace(origin, std::make_unique<NativeIOHost>(
                                   this, origin, std::move(origin_root_path)));
    DCHECK(insert_succeeded);
  }

  // base::Unretained is safe here because this NativeIOManager owns the
  // NativeIOHost. So, the unretained NativeIOManager is guaranteed to outlive
  // the  NativeIOHost and the closure that it uses.
  it->second->DeleteAllData(
      base::BindOnce(&NativeIOManager::OnDeleteOriginDataCompleted,
                     base::Unretained(this), std::move(callback)));
}

base::FilePath NativeIOManager::RootPathForOrigin(const url::Origin& origin) {
  // TODO(pwnall): Implement in-memory files instead of bouncing in incognito.
  if (root_path_.empty())
    return root_path_;

  std::string origin_identifier = storage::GetIdentifierFromOrigin(origin);
  base::FilePath origin_path = root_path_.AppendASCII(origin_identifier);
  DCHECK(root_path_.IsParent(origin_path));
  return origin_path;
}

// static
base::FilePath NativeIOManager::GetNativeIORootPath(
    const base::FilePath& profile_root) {
  if (profile_root.empty())
    return base::FilePath();

  return profile_root.Append(kNativeIODirectoryName);
}

// static
blink::mojom::NativeIOErrorPtr NativeIOManager::FileErrorToNativeIOError(
    base::File::Error file_error,
    std::string message) {
  blink::mojom::NativeIOErrorType native_io_error_type =
      blink::native_io::FileErrorToNativeIOErrorType(file_error);
  std::string final_message =
      message.empty()
          ? blink::native_io::GetDefaultMessage(native_io_error_type)
          : message;
  return blink::mojom::NativeIOError::New(native_io_error_type, final_message);
}

}  // namespace content
