// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/host/weights_file_creator_impl.h"

#include <utility>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/webnn/host/weights_file_provider.h"
#include "services/webnn/host/weights_file_session_impl.h"

namespace webnn {

// static
void WeightsFileCreatorImpl::Create(
    mojo::PendingReceiver<mojom::WebNNWeightsFileCreator> receiver,
    const url::Origin& origin,
    bool is_incognito) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<WeightsFileCreatorImpl>(origin, is_incognito),
      std::move(receiver));
}

WeightsFileCreatorImpl::WeightsFileCreatorImpl(const url::Origin& origin,
                                               bool is_incognito)
    : origin_(origin), is_incognito_(is_incognito) {}

WeightsFileCreatorImpl::~WeightsFileCreatorImpl() = default;

void WeightsFileCreatorImpl::OpenWeightsFile(OpenWeightsFileCallback callback) {
  if (is_incognito_) {
    std::move(callback).Run(/*writable_fd=*/base::File(),
                            /*session=*/mojo::NullRemote());
    return;
  }

  // Create the tempfile; capacity is enforced incrementally by the session.
  // Keep the path so `WeightsFileSessionImpl::Finalize` can reopen it
  // read-only.
  webnn::CreateWeightsFileWithPath(
      base::BindOnce(&WeightsFileCreatorImpl::DidOpenWeightsFile,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void WeightsFileCreatorImpl::DidOpenWeightsFile(
    OpenWeightsFileCallback callback,
    base::File file,
    base::FilePath path) {
  if (!file.IsValid()) {
    std::move(callback).Run(/*writable_fd=*/base::File(),
                            /*session=*/mojo::NullRemote());
    return;
  }

  // Dup a writable fd for the renderer before handing the original to the
  // session.
  base::File renderer_fd = file.Duplicate();
  if (!renderer_fd.IsValid()) {
    file.Close();
    base::DeleteFile(path);
    std::move(callback).Run(/*writable_fd=*/base::File(),
                            /*session=*/mojo::NullRemote());
    return;
  }

  mojo::PendingRemote<mojom::WeightsFileSession> session_remote;
  auto session_receiver = session_remote.InitWithNewPipeAndPassReceiver();

  // The session does blocking file I/O (`fstat`, open, unlink, close), so
  // bind it on a MayBlock sequence rather than this (UI) thread. The renderer
  // gets its remote immediately; sync calls are buffered until the bind lands.
  base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})
      ->PostTask(FROM_HERE,
                 base::BindOnce(&WeightsFileSessionImpl::Create,
                                std::move(session_receiver), std::move(file),
                                std::move(path), origin_));

  std::move(callback).Run(std::move(renderer_fd), std::move(session_remote));
}

}  // namespace webnn
