// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_LAZY_SHARED_URL_LOADER_FACTORY_H_
#define CONTENT_COMMON_LAZY_SHARED_URL_LOADER_FACTORY_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "content/common/content_export.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace content {

// LazySharedURLLoaderFactory is a thread-safe, deadlock-free wrapper around
// network::SharedURLLoaderFactory.
//
// RATIONALE:
// During page load / navigation commit, the renderer main thread is highly
// congested. Proactively cloning the main thread's URLLoaderFactory bundle
// (e.g., for ServiceWorker network provider fallback or worker thread startup)
// adds significant Mojo IPC overhead (5-10+ IPCs per clone) on the critical
// path, even if the page/worker never makes a network request.
//
// DESIGN:
// This class defers the actual Mojo `Clone` IPC until the first network
// request (or Mojo Clone request) is initiated.
//
// To prevent deadlocks, this class operates asynchronously by default:
// 1. Any requests made before the real factory is bound are buffered in a
// queue.
// 2. A task is posted to the main thread to perform the actual Mojo clone.
// 3. Once the cloned factory is sent back, the buffered requests are flushed,
//    and subsequent requests are forwarded directly.
//
// As an optimization, if the factory is used on the main thread sequence, the
// cloning and binding happen synchronously to avoid any task-hop latency.
//
// THREADING:
// - Created on the target sequence (main thread or worker thread) via
//   LazyPendingSharedURLLoaderFactory.
// - Interacts with the main thread asynchronously if used on a background
// thread.
// - Performs synchronous cloning and binding if used on the main thread.
// - Cleaned up safely on destruction: if destroyed on a background sequence,
//   the destructor posts the clone callback back to the main thread to be
//   safely released there (preventing sequence checker crashes on captured
//   resources).
class CONTENT_EXPORT LazySharedURLLoaderFactory
    : public network::SharedURLLoaderFactory {
 public:
  // Callback that runs on the MAIN thread to clone the actual factory bundle.
  // It returns a PendingSharedURLLoaderFactory which is safe to pass across
  // threads.
  using CloneCallback = base::RepeatingCallback<
      std::unique_ptr<network::PendingSharedURLLoaderFactory>()>;

  LazySharedURLLoaderFactory(
      scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner,
      CloneCallback clone_callback);

  // network::SharedURLLoaderFactory implementation:

  // Triggers the lazy cloning process if not already started, buffers the
  // request, and flushes it once the real factory is bound.
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> loader,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override;

  // Handles Mojo Clone requests (mojom::URLLoaderFactory). These are also
  // buffered and flushed once the real factory is bound.
  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver)
      override;

  // Handles C++ Clone requests. Returns a new LazyPendingSharedURLLoaderFactory
  // wrapping the same main thread clone callback, preserving laziness.
  std::unique_ptr<network::PendingSharedURLLoaderFactory> Clone() override;

 private:
  friend class base::RefCountedThreadSafe<LazySharedURLLoaderFactory>;
  ~LazySharedURLLoaderFactory() override;

  // Structure to hold all necessary parameters for a deferred URLLoader
  // request.
  struct PendingRequest {
    mojo::PendingReceiver<network::mojom::URLLoader> loader;
    int32_t request_id;
    uint32_t options;
    network::ResourceRequest request;
    mojo::PendingRemote<network::mojom::URLLoaderClient> client;
    net::MutableNetworkTrafficAnnotationTag traffic_annotation;
  };

  // Posts a task to the main thread to trigger the clone callback.
  void TriggerMainThreadClone();

  // Called on the worker thread when the main thread has completed the clone.
  // Instantiates the real factory and flushes all buffered requests.
  void OnBindComplete(
      std::unique_ptr<network::PendingSharedURLLoaderFactory> pending_factory);

  // Static helper that runs on the MAIN thread to execute the clone callback
  // and send the result back to the worker thread.
  static void CloneOnMainThread(
      scoped_refptr<base::SequencedTaskRunner> worker_task_runner,
      base::WeakPtr<LazySharedURLLoaderFactory> self,
      CloneCallback clone_callback);

  // Task runners for thread hopping.
  scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> worker_task_runner_;

  // The callback to run on the main thread.
  CloneCallback clone_callback_;

  // True if we have already posted a task to the main thread to clone.
  bool binding_in_progress_ = false;

  // The actual factory, once bound. nullptr initially.
  scoped_refptr<network::SharedURLLoaderFactory> real_factory_;

  // Buffers for requests received before binding completes.
  std::vector<PendingRequest> pending_requests_;
  std::vector<mojo::PendingReceiver<network::mojom::URLLoaderFactory>>
      pending_clone_receivers_;

  base::WeakPtrFactory<LazySharedURLLoaderFactory> weak_factory_{this};
};

// LazyPendingSharedURLLoaderFactory is the thread-safe container used to pass
// the lazy factory setup across threads (e.g., from main thread to worker
// thread). When CreateFactory() is called on the target thread, it instantiates
// the LazySharedURLLoaderFactory.
class CONTENT_EXPORT LazyPendingSharedURLLoaderFactory
    : public network::PendingSharedURLLoaderFactory {
 public:
  using CloneCallback = base::RepeatingCallback<
      std::unique_ptr<network::PendingSharedURLLoaderFactory>()>;

  // Always require the main thread task runner explicitly to avoid deprecated
  // implicit thread-capturing APIs.
  LazyPendingSharedURLLoaderFactory(
      scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner,
      CloneCallback clone_callback);

  ~LazyPendingSharedURLLoaderFactory() override;

 protected:
  // network::PendingSharedURLLoaderFactory implementation:
  // Instantiates the LazySharedURLLoaderFactory on the calling (worker) thread.
  scoped_refptr<network::SharedURLLoaderFactory> CreateFactory() override;

 private:
  scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner_;
  CloneCallback clone_callback_;
};

// Helper function to create a LazyPendingSharedURLLoaderFactory that lazily
// clones the given SharedURLLoaderFactory.
CONTENT_EXPORT std::unique_ptr<network::PendingSharedURLLoaderFactory>
CreateLazyPendingURLLoaderFactory(
    scoped_refptr<network::SharedURLLoaderFactory> factory_to_clone,
    scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner);

}  // namespace content

#endif  // CONTENT_COMMON_LAZY_SHARED_URL_LOADER_FACTORY_H_
