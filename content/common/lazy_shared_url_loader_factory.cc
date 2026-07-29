// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/lazy_shared_url_loader_factory.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/url_loader.mojom.h"

namespace content {

using base::SequencedTaskRunner;

namespace {

// Safely releases `callback` (which may capture main-thread-bound references)
// on the main thread if destroyed on a background sequence.
void ReleaseCallbackOnMainThread(
    scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner,
    LazyPendingSharedURLLoaderFactory::CloneCallback callback) {
  if (callback && !main_thread_task_runner->RunsTasksInCurrentSequence()) {
    main_thread_task_runner->DeleteSoon(
        FROM_HERE,
        std::make_unique<LazyPendingSharedURLLoaderFactory::CloneCallback>(
            std::move(callback)));
  }
}

}  // namespace

LazySharedURLLoaderFactory::LazySharedURLLoaderFactory(
    scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner,
    CloneCallback clone_callback)
    : main_thread_task_runner_(std::move(main_thread_task_runner)),
      worker_task_runner_(SequencedTaskRunner::GetCurrentDefault()),
      clone_callback_(std::move(clone_callback)) {
  // We must be instantiated on the worker thread where we will be used.
  CHECK(worker_task_runner_);
}

LazySharedURLLoaderFactory::~LazySharedURLLoaderFactory() {
  ReleaseCallbackOnMainThread(main_thread_task_runner_,
                              std::move(clone_callback_));
}

void LazySharedURLLoaderFactory::CreateLoaderAndStart(
    mojo::PendingReceiver<network::mojom::URLLoader> loader,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  CHECK(worker_task_runner_->RunsTasksInCurrentSequence());

  // If the real factory is already bound, bypass the buffering and forward
  // directly.
  if (real_factory_) {
    real_factory_->CreateLoaderAndStart(std::move(loader), request_id, options,
                                        request, std::move(client),
                                        traffic_annotation);
    return;
  }

  // Buffer the request parameters. They will be flushed in OnBindComplete().
  pending_requests_.push_back(
      PendingRequest{std::move(loader), request_id, options, request,
                     std::move(client), traffic_annotation});

  // Start the asynchronous main-thread clone process.
  TriggerMainThreadClone();
}

void LazySharedURLLoaderFactory::Clone(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver) {
  CHECK(worker_task_runner_->RunsTasksInCurrentSequence());

  // Forward immediately if already bound.
  if (real_factory_) {
    real_factory_->Clone(std::move(receiver));
    return;
  }

  // Buffer the Mojo receiver.
  pending_clone_receivers_.push_back(std::move(receiver));

  // Start the asynchronous main-thread clone process.
  TriggerMainThreadClone();
}

std::unique_ptr<network::PendingSharedURLLoaderFactory>
LazySharedURLLoaderFactory::Clone() {
  CHECK(worker_task_runner_->RunsTasksInCurrentSequence());
  if (real_factory_) {
    return real_factory_->Clone();
  }
  // To preserve laziness when cloning this factory (e.g., for nested workers),
  // we return a new LazyPendingSharedURLLoaderFactory that carries the same
  // main thread task runner and clone callback.
  return std::make_unique<LazyPendingSharedURLLoaderFactory>(
      main_thread_task_runner_, clone_callback_);
}

void LazySharedURLLoaderFactory::TriggerMainThreadClone() {
  // Avoid posting multiple redundant tasks to the main thread if one is already
  // in flight.
  if (binding_in_progress_) {
    return;
  }

  binding_in_progress_ = true;

  if (main_thread_task_runner_->RunsTasksInCurrentSequence()) {
    // If we are already on the main thread, perform the clone and bind
    // synchronously without any task-hop overhead.
    std::unique_ptr<network::PendingSharedURLLoaderFactory> pending_factory =
        clone_callback_.Run();
    OnBindComplete(std::move(pending_factory));
    return;
  }

  // Hop to the main thread to perform the clone (which must happen on the main
  // thread because Mojo remotes are thread-bound to the main thread).
  using SafeCloneCallbackPtr =
      std::unique_ptr<LazyPendingSharedURLLoaderFactory::CloneCallback,
                      base::OnTaskRunnerDeleter>;

  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<base::SequencedTaskRunner> worker_task_runner,
             base::WeakPtr<LazySharedURLLoaderFactory> self,
             SafeCloneCallbackPtr callback_ptr) {
            CloneOnMainThread(std::move(worker_task_runner), std::move(self),
                              std::move(*callback_ptr));
            delete callback_ptr.release();
          },
          worker_task_runner_, weak_factory_.GetWeakPtr(),
          SafeCloneCallbackPtr(
              new LazyPendingSharedURLLoaderFactory::CloneCallback(
                  clone_callback_),
              base::OnTaskRunnerDeleter(main_thread_task_runner_))));
}

// static
void LazySharedURLLoaderFactory::CloneOnMainThread(
    scoped_refptr<base::SequencedTaskRunner> worker_task_runner,
    base::WeakPtr<LazySharedURLLoaderFactory> self,
    CloneCallback clone_callback) {
  // Run the callback to perform the actual Mojo Clone IPCs.
  std::unique_ptr<network::PendingSharedURLLoaderFactory> pending_factory =
      std::move(clone_callback).Run();

  // Post the result back to the worker thread directly.
  worker_task_runner->PostTask(
      FROM_HERE, base::BindOnce(&LazySharedURLLoaderFactory::OnBindComplete,
                                self, std::move(pending_factory)));
}

void LazySharedURLLoaderFactory::OnBindComplete(
    std::unique_ptr<network::PendingSharedURLLoaderFactory> pending_factory) {
  CHECK(worker_task_runner_->RunsTasksInCurrentSequence());

  // Reconstruct the real SharedURLLoaderFactory from the pending transport
  // container.
  if (pending_factory) {
    real_factory_ =
        network::SharedURLLoaderFactory::Create(std::move(pending_factory));
  }

  binding_in_progress_ = false;

  // 1. Flush all buffered resource load requests.
  for (auto& req : pending_requests_) {
    if (real_factory_) {
      real_factory_->CreateLoaderAndStart(
          std::move(req.loader), req.request_id, req.options, req.request,
          std::move(req.client), req.traffic_annotation);
    } else {
      // If we failed to bind (e.g., frame was destroyed), fail the pending
      // requests immediately.
      if (req.client.is_valid()) {
        mojo::Remote<network::mojom::URLLoaderClient> client(
            std::move(req.client));
        client->OnComplete(network::URLLoaderCompletionStatus(net::ERR_FAILED));
      }
    }
  }
  pending_requests_.clear();

  // 2. Flush all buffered Mojo Clone requests.
  for (auto& receiver : pending_clone_receivers_) {
    if (real_factory_) {
      real_factory_->Clone(std::move(receiver));
    }
  }
  pending_clone_receivers_.clear();
}

// -----------------------------------------------------------------------------

LazyPendingSharedURLLoaderFactory::LazyPendingSharedURLLoaderFactory(
    scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner,
    CloneCallback clone_callback)
    : main_thread_task_runner_(std::move(main_thread_task_runner)),
      clone_callback_(std::move(clone_callback)) {
  CHECK(main_thread_task_runner_);
}

LazyPendingSharedURLLoaderFactory::~LazyPendingSharedURLLoaderFactory() {
  ReleaseCallbackOnMainThread(main_thread_task_runner_,
                              std::move(clone_callback_));
}

scoped_refptr<network::SharedURLLoaderFactory>
LazyPendingSharedURLLoaderFactory::CreateFactory() {
  // This is called on the worker thread, instantiating the lazy factory.
  return base::MakeRefCounted<LazySharedURLLoaderFactory>(
      main_thread_task_runner_, clone_callback_);
}

std::unique_ptr<network::PendingSharedURLLoaderFactory>
CreateLazyPendingURLLoaderFactory(
    scoped_refptr<network::SharedURLLoaderFactory> factory_to_clone,
    scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner) {
  CHECK(factory_to_clone);
  CHECK(main_thread_task_runner);
  return std::make_unique<LazyPendingSharedURLLoaderFactory>(
      std::move(main_thread_task_runner),
      base::BindRepeating(
          [](scoped_refptr<network::SharedURLLoaderFactory> factory)
              -> std::unique_ptr<network::PendingSharedURLLoaderFactory> {
            return factory->Clone();
          },
          std::move(factory_to_clone)));
}

}  // namespace content
