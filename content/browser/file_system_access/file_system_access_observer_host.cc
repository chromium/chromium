// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_system_access_observer_host.h"

#include "base/feature_list.h"
#include "content/browser/file_system_access/file_system_access_error.h"
#include "content/browser/file_system_access/file_system_access_manager_impl.h"
#include "content/browser/file_system_access/file_system_access_watcher_manager.h"
#include "content/public/browser/file_system_access_permission_context.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_observer.mojom.h"

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

  // TODO(https://crbug.com/1019297): Add this flag to chrome://flags.
  CHECK(base::FeatureList::IsEnabled(blink::features::kFileSystemObserver));

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

  mojo::PendingRemote<blink::mojom::FileSystemAccessObserver> observer_remote;
  mojo::PendingReceiver<blink::mojom::FileSystemAccessObserver>
      observer_receiver = observer_remote.InitWithNewPipeAndPassReceiver();

  // TODO(https://crbug.com/1019297): Actually watch the file path.

  observer_remotes_.Add(std::move(observer_remote));

  std::move(callback).Run(file_system_access_error::Ok(),
                          std::move(observer_receiver));
}

void FileSystemAccessObserverHost::Unobserve(
    mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken> token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(manager_);

  // TODO(https://crbug.com/1019297): Implement this.
  NOTIMPLEMENTED();
}

void FileSystemAccessObserverHost::OnHostReceiverDisconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observer_remotes_.Clear();
  host_receiver_.reset();

  // Destroys `this`.
  watcher_manager_->RemoveObserverHost(this);
}

}  // namespace content
