// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NETWORK_CROSS_THREAD_NETWORK_CONTEXT_H_
#define CONTENT_BROWSER_NETWORK_CROSS_THREAD_NETWORK_CONTEXT_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "base/unguessable_token.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/reconnect_notifier.h"
#include "services/network/public/mojom/connection_change_observer_client.mojom-forward.h"
#include "services/network/public/mojom/fetch_api.mojom-forward.h"
#include "services/network/public/mojom/network_context.mojom-forward.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

namespace mojo {
template <typename Interface>
class Remote;
}  // namespace mojo

namespace net {
struct MutableNetworkTrafficAnnotationTag;
class NetworkAnonymizationKey;
}  // namespace net

namespace content {

class StoragePartitionImpl;

// A helper class that enables background sequences in the Browser process to
// access the NetworkContext directly, avoiding UI thread hops for each call.
//
// While instances of this class are ref-counted and safe to pass across threads
// (RefCountedThreadSafe), method calls permanently bind to the single sequence
// that first invokes a method on this instance. If multiple sequences need
// access to the NetworkContext, each sequence must obtain its own
// CrossThreadNetworkContext instance.
//
// On construction (on the UI thread), it clones the StoragePartition's
// NetworkContext into a PendingRemote so that the initial call on the
// background sequence can bind and dispatch directly to the NetworkService
// without a UI thread task hop. It also handles reconnection after
// NetworkService crashes via pipe-buffering: when disconnected, it creates a
// new pipe locally, posts a task to the UI thread to re-bind it to the
// StoragePartition's NetworkContext, and buffers any outgoing Mojo messages
// until the pipe is connected.
class CONTENT_EXPORT CrossThreadNetworkContext
    : public base::RefCountedThreadSafe<CrossThreadNetworkContext> {
 public:
  // Callback invoked on the UI thread to bind a pending NetworkContext
  // receiver to the underlying NetworkContext.
  using NetworkContextBinder = base::RepeatingCallback<void(
      mojo::PendingReceiver<network::mojom::NetworkContext>)>;

  // Must be called on the UI thread.
  static scoped_refptr<CrossThreadNetworkContext> Create(
      base::WeakPtr<StoragePartitionImpl> storage_partition);

  // Factory for testing. Must be called on the sequence that runs
  // `ui_task_runner`.
  static scoped_refptr<CrossThreadNetworkContext> CreateForTesting(
      NetworkContextBinder network_context_binder,
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner);

  CrossThreadNetworkContext(const CrossThreadNetworkContext&) = delete;
  CrossThreadNetworkContext& operator=(const CrossThreadNetworkContext&) =
      delete;

  // Sends preconnect requests directly to the NetworkContext from the bound
  // sequence.
  void PreconnectSockets(
      uint32_t num_streams,
      const GURL& url,
      network::mojom::CredentialsMode credentials_mode,
      const net::NetworkAnonymizationKey& network_anonymization_key,
      const base::UnguessableToken& network_restrictions_id,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      const std::optional<net::ConnectionKeepAliveConfig>& keepalive_config,
      mojo::PendingRemote<network::mojom::ConnectionChangeObserverClient>
          connection_change_observer_client);

  // Adds QUIC hints directly to the NetworkContext from the bound sequence.
  void AddQuicHints(
      const std::vector<url::SchemeHostPort>& origins,
      const net::NetworkAnonymizationKey& network_anonymization_key);

  // Flushes the underlying remote asynchronously on the bound sequence.
  // For testing only.
  void FlushAsyncForTesting(base::OnceClosure callback);

 private:
  friend class base::RefCountedThreadSafe<CrossThreadNetworkContext>;

  CrossThreadNetworkContext(
      NetworkContextBinder network_context_binder,
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner);
  ~CrossThreadNetworkContext();

  network::mojom::NetworkContext* EnsureNetworkContext();

  const NetworkContextBinder network_context_binder_;
  const scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;

  mojo::PendingRemote<network::mojom::NetworkContext> pending_network_context_
      GUARDED_BY_CONTEXT(sequence_checker_);
  scoped_refptr<base::SequencedTaskRunner> bound_task_runner_
      GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<mojo::Remote<network::mojom::NetworkContext>> network_context_
      GUARDED_BY_CONTEXT(sequence_checker_);
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_NETWORK_CROSS_THREAD_NETWORK_CONTEXT_H_
