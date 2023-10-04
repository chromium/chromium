// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_system_access_observer_host.h"

#include <memory>

#include "base/ranges/algorithm.h"
#include "content/browser/file_system_access/file_system_access_directory_handle_impl.h"
#include "content/browser/file_system_access/file_system_access_error.h"
#include "content/browser/file_system_access/file_system_access_file_handle_impl.h"
#include "content/browser/file_system_access/file_system_access_manager_impl.h"
#include "content/browser/file_system_access/file_system_access_observer_observation.h"
#include "content/browser/file_system_access/file_system_access_transfer_token_impl.h"
#include "content/browser/file_system_access/file_system_access_watcher_manager.h"
#include "content/public/browser/file_system_access_permission_context.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "storage/browser/file_system/file_system_url.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_observer.mojom.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom-shared.h"

namespace content {

using HandleType = FileSystemAccessPermissionContext::HandleType;

FileSystemAccessObserverHost::FileSystemAccessObserverHost(
    FileSystemAccessManagerImpl* manager,
    FileSystemAccessWatcherManager* watcher_manager,
    const BindingContext& binding_context,
    mojo::PendingReceiver<blink::mojom::FileSystemAccessObserverHost>
        host_receiver)
    : manager_(manager),
      watcher_manager_(watcher_manager),
      binding_context_(binding_context),
      host_receiver_(this, std::move(host_receiver)) {
  CHECK(manager_);
  CHECK(watcher_manager_);

  // `base::Unretained` is safe here because this instance owns
  // `host_receiver_`.
  host_receiver_.set_disconnect_handler(
      base::BindOnce(&FileSystemAccessObserverHost::OnHostReceiverDisconnect,
                     base::Unretained(this)));
}

FileSystemAccessObserverHost::~FileSystemAccessObserverHost() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void FileSystemAccessObserverHost::Observe(
    mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken> token,
    bool is_recursive,
    ObserveCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(manager_);

  manager_->ResolveTransferToken(
      std::move(token),
      base::BindOnce(
          &FileSystemAccessObserverHost::DidResolveTransferTokenToObserve,
          weak_factory_.GetWeakPtr(), is_recursive, std::move(callback)));
}

void FileSystemAccessObserverHost::DidResolveTransferTokenToObserve(
    bool is_recursive,
    ObserveCallback callback,
    FileSystemAccessTransferTokenImpl* resolved_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!resolved_token) {
    std::move(callback).Run(
        file_system_access_error::FromStatus(
            blink::mojom::FileSystemAccessStatus::kInvalidArgument),
        mojo::NullReceiver());
    return;
  }

  if (resolved_token->GetReadGrant()->GetStatus() !=
      blink::mojom::PermissionStatus::GRANTED) {
    std::move(callback).Run(
        file_system_access_error::FromStatus(
            blink::mojom::FileSystemAccessStatus::kPermissionDenied),
        mojo::NullReceiver());
    return;
  }

  switch (resolved_token->type()) {
    case FileSystemAccessPermissionContext::HandleType::kDirectory:
      watcher_manager()->GetDirectoryObservation(
          resolved_token->url(), is_recursive,
          base::BindOnce(
              &FileSystemAccessObserverHost::GotObservation,
              weak_factory_.GetWeakPtr(),
              resolved_token->CreateDirectoryHandle(binding_context()),
              std::move(callback)));
      break;
    case FileSystemAccessPermissionContext::HandleType::kFile:
      watcher_manager()->GetFileObservation(
          resolved_token->url(),
          base::BindOnce(&FileSystemAccessObserverHost::GotObservation,
                         weak_factory_.GetWeakPtr(),
                         resolved_token->CreateFileHandle(binding_context()),
                         std::move(callback)));
      break;
  }
}

void FileSystemAccessObserverHost::Unobserve(
    mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken> token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(manager_);

  if (observations_.empty()) {
    return;
  }

  manager_->ResolveTransferToken(
      std::move(token),
      base::BindOnce(
          &FileSystemAccessObserverHost::DidResolveTransferTokenToUnobserve,
          weak_factory_.GetWeakPtr()));
}

void FileSystemAccessObserverHost::DidResolveTransferTokenToUnobserve(
    FileSystemAccessTransferTokenImpl* resolved_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!resolved_token) {
    return;
  }

  // TODO(https://crbug.com/1489057): Better handle overlapping observations.
  base::EraseIf(observations_, [&](const auto& observation) {
    return observation->handle_url() == resolved_token->url();
  });
}

void FileSystemAccessObserverHost::GotObservation(
    absl::variant<std::unique_ptr<FileSystemAccessDirectoryHandleImpl>,
                  std::unique_ptr<FileSystemAccessFileHandleImpl>> handle,
    ObserveCallback callback,
    base::expected<std::unique_ptr<FileSystemAccessWatcherManager::Observation>,
                   blink::mojom::FileSystemAccessErrorPtr>
        observation_or_error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!observation_or_error.has_value()) {
    std::move(callback).Run(std::move(observation_or_error.error()),
                            mojo::NullReceiver());
    return;
  }

  mojo::PendingRemote<blink::mojom::FileSystemAccessObserver> observer_remote;
  mojo::PendingReceiver<blink::mojom::FileSystemAccessObserver>
      observer_receiver = observer_remote.InitWithNewPipeAndPassReceiver();

  auto observer_observation =
      std::make_unique<FileSystemAccessObserverObservation>(
          this, std::move(observation_or_error.value()),
          std::move(observer_remote), std::move(handle));
  observations_.insert(std::move(observer_observation));

  std::move(callback).Run(file_system_access_error::Ok(),
                          std::move(observer_receiver));
}

void FileSystemAccessObserverHost::RemoveObservation(
    FileSystemAccessObserverObservation* observation) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  size_t count_removed = observations_.erase(observation);
  CHECK_EQ(count_removed, 1u);
}

void FileSystemAccessObserverHost::OnHostReceiverDisconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observations_.clear();
  host_receiver_.reset();

  // Destroys `this`.
  watcher_manager_->RemoveObserverHost(this);
}

}  // namespace content
