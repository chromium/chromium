// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_URL_LOADER_FACTORY_GETTER_H_
#define CONTENT_BROWSER_URL_LOADER_FACTORY_GETTER_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace content {

class StoragePartitionImpl;

// SharedURLLoaderFactory that caches and reuses a URLLoaderFactory remote
// created by its `CreateCallback`, and re-create and reconnect if the cached
// remote is disconnected.
// All methods (including the `CreateCallback` and the destructor) must be/are
// called on the original sequence.
// TODO(crbug.com/40947547): Merge `URLLoaderFactoryGetter` and rename the file.
class CONTENT_EXPORT ReconnectableURLLoaderFactory
    : public network::SharedURLLoaderFactory {
 public:
  // On success:
  //   Creates and assigns a new URLLoaderFactory to the output parameter.
  //
  // On failure (e.g., underlying network context is gone):
  //   Leaves the output parameter unchanged.
  //   Cancellation of the callback is usually achieved by binding a WeakPtr to
  //   it in the caller's context.
  using CreateCallback = base::RepeatingCallback<void(
      mojo::PendingRemote<network::mojom::URLLoaderFactory>*)>;

  // The constructor doesn't call `create_url_loader_factory_callback`
  // synchronously.
  explicit ReconnectableURLLoaderFactory(
      CreateCallback create_url_loader_factory_callback);

  ReconnectableURLLoaderFactory(const ReconnectableURLLoaderFactory&) = delete;
  ReconnectableURLLoaderFactory& operator=(
      const ReconnectableURLLoaderFactory&) = delete;

  void Reset();
  void FlushForTesting();

  // mojom::URLLoaderFactory implementation:
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& url_request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override;
  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver)
      override;

  // SharedURLLoaderFactory implementation:
  std::unique_ptr<network::PendingSharedURLLoaderFactory> Clone() override;

 private:
  friend class base::RefCounted<ReconnectableURLLoaderFactory>;
  ~ReconnectableURLLoaderFactory() override;

  // URLLoaderFactory caching mechanism only accessed from the original thread.
  network::mojom::URLLoaderFactory* GetURLLoaderFactory();
  mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory_;
  bool is_test_url_loader_factory_ = false;

  const CreateCallback create_url_loader_factory_callback_;

  SEQUENCE_CHECKER(sequence_checker_);
};

// Holds on to URLLoaderFactory for a given StoragePartition and allows code
// running on the IO thread to access them. Note these are the factories used by
// the browser process for frame requests.
class URLLoaderFactoryGetter
    : public base::RefCountedThreadSafe<URLLoaderFactoryGetter,
                                        BrowserThread::DeleteOnIOThread> {
 public:
  CONTENT_EXPORT URLLoaderFactoryGetter();

  URLLoaderFactoryGetter(const URLLoaderFactoryGetter&) = delete;
  URLLoaderFactoryGetter& operator=(const URLLoaderFactoryGetter&) = delete;

  // Initializes this object on the UI thread. The |partition| is used to
  // initialize the URLLoaderFactories for the network service, and
  // ServiceWorkers, and will be cached to recover from connection error.
  // After Initialize(), you can get URLLoaderFactories from this
  // getter.
  void Initialize(StoragePartitionImpl* partition);

  // Clear the cached pointer to |StoragePartitionImpl| on the UI thread. Should
  // be called when the partition is going away.
  void OnStoragePartitionDestroyed();

  // Called on the UI thread to create a PendingSharedURLLoaderFactory that
  // holds a reference to this URLLoaderFactoryGetter, which can be used on IO
  // thread to construct a SharedURLLoaderFactory that can be used to access the
  // URLLoaderFactory to the network service and supports auto-reconnect after
  // crash.
  CONTENT_EXPORT std::unique_ptr<network::PendingSharedURLLoaderFactory>
  GetPendingNetworkFactory();

  // Overrides the network URLLoaderFactory for subsequent requests. Passing a
  // null pointer will restore the default behavior.
  CONTENT_EXPORT void SetNetworkFactoryForTesting(
      network::mojom::URLLoaderFactory* test_factory);

  CONTENT_EXPORT mojo::Remote<network::mojom::URLLoaderFactory>*
  original_network_factory_for_testing() {
    return &network_factory_;
  }

  // When this global function is set, if GetURLLoaderFactory is called and
  // |test_factory_| is null, then the callback will be run. This method must be
  // called either on the IO thread or before threads start. This callback is
  // run on the IO thread.
  using GetNetworkFactoryCallback = base::RepeatingCallback<void(
      scoped_refptr<URLLoaderFactoryGetter> url_loader_factory_getter)>;
  CONTENT_EXPORT static void SetGetNetworkFactoryCallbackForTesting(
      const GetNetworkFactoryCallback& get_network_factory_callback);

  // Call |network_factory_.FlushForTesting()| on IO thread. For test use only.
  CONTENT_EXPORT void FlushNetworkInterfaceOnIOThreadForTesting();

 private:
  class PendingURLLoaderFactoryForIOThread;
  class URLLoaderFactoryForIOThread;

  friend class base::DeleteHelper<URLLoaderFactoryGetter>;
  friend struct BrowserThread::DeleteOnThread<BrowserThread::IO>;

  CONTENT_EXPORT ~URLLoaderFactoryGetter();
  void InitializeOnIOThread(
      mojo::PendingRemote<network::mojom::URLLoaderFactory> network_factory);

  // Moves |network_factory| to |network_factory_| and sets up an error handler.
  void ReinitializeOnIOThread(
      mojo::Remote<network::mojom::URLLoaderFactory> network_factory);

  // Send |network_factory_request| to cached |StoragePartitionImpl|.
  void HandleNetworkFactoryRequestOnUIThread(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory>
          network_factory_receiver);

  // Called on the IO thread to get the URLLoaderFactory to the network service.
  // The pointer shouldn't be cached.
  network::mojom::URLLoaderFactory* GetURLLoaderFactory();

  // Call |network_factory_.FlushForTesting()|. For test use only. When the
  // flush is complete, |callback| will be called.
  void FlushNetworkInterfaceForTesting(base::OnceClosure callback);

  // Only accessed on IO thread.
  mojo::Remote<network::mojom::URLLoaderFactory> network_factory_;
  raw_ptr<network::mojom::URLLoaderFactory> test_factory_ = nullptr;

  // Used to re-create |network_factory_| when connection error happens. Can
  // only be accessed on UI thread. Must be cleared by |StoragePartitionImpl|
  // when it's going away.
  raw_ptr<StoragePartitionImpl> partition_ = nullptr;
};

}  // namespace content

#endif  // CONTENT_BROWSER_URL_LOADER_FACTORY_GETTER_H_
