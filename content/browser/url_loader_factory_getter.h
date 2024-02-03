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
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace network {
class PendingSharedURLLoaderFactory;
class SharedURLLoaderFactory;
}  // namespace network

namespace content {

class StoragePartitionImpl;

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

  // Called on the IO thread to get a shared wrapper to this
  // URLLoaderFactoryGetter, which can be used to access the URLLoaderFactory
  // to the network service and supports auto-reconnect after crash.
  CONTENT_EXPORT scoped_refptr<network::SharedURLLoaderFactory>
  GetNetworkFactory();

  // Called on the UI thread to get an info that holds a reference to this
  // URLLoaderFactoryGetter, which can be used to construct a similar
  // SharedURLLoaderFactory as returned from |GetNetworkFactory()| on IO thread.
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
