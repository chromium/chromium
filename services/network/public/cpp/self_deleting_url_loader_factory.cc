// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/self_deleting_url_loader_factory.h"

#include <utility>

#include "base/check.h"

namespace network {

SelfDeletingURLLoaderFactory::SelfDeletingURLLoaderFactory(
    mojo::PendingReceiver<mojom::URLLoaderFactory> factory_receiver,
    base::SelfDeletingPassKey key,
    scoped_refptr<base::SequencedTaskRunner> receiver_task_runner)
    : base::SelfDeleting(key),
      receiver_task_runner_(std::move(receiver_task_runner)) {
  // Mojo requires a receiver task runner to run on the owning sequence; the
  // runner only picks the task queue used to dispatch messages on it.
  CHECK(!receiver_task_runner_ ||
        receiver_task_runner_->RunsTasksInCurrentSequence());
  receivers_.set_disconnect_handler(base::BindRepeating(
      &SelfDeletingURLLoaderFactory::OnDisconnect, base::Unretained(this)));
  receivers_.Add(this, std::move(factory_receiver), receiver_task_runner_);
}

SelfDeletingURLLoaderFactory::~SelfDeletingURLLoaderFactory() = default;

void SelfDeletingURLLoaderFactory::DisconnectReceiversAndDestroy() {
  // Clear |receivers_| to explicitly make sure that no further method
  // invocations or disconnection notifications will happen.  (per the
  // comment of mojo::ReceiverSet::Clear)
  receivers_.Clear();

  // Similarly to OnDisconnect, if there are no more |receivers_|, then no
  // instance methods of |this| can be called in the future (mojo methods Clone
  // and CreateLoaderAndStart should be the only public entrypoints).
  // Therefore, it is safe to delete |this| at this point.
  delete this;
}

void SelfDeletingURLLoaderFactory::Clone(
    mojo::PendingReceiver<mojom::URLLoaderFactory> loader) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  receivers_.Add(this, std::move(loader), receiver_task_runner_);
}

void SelfDeletingURLLoaderFactory::ReportBadMessage(std::string_view message) {
  receivers_.ReportBadMessage(message);
  if (receivers_.empty()) {
    delete this;
  }
}

void SelfDeletingURLLoaderFactory::OnDisconnect() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (receivers_.empty()) {
    // If there are no more |receivers_|, then no instance methods of |this| can
    // be called in the future (mojo methods Clone and CreateLoaderAndStart
    // should be the only public entrypoints).  Therefore, it is safe to delete
    // |this| at this point.
    delete this;
  }
}

}  // namespace network
