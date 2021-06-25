// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_URL_LOADER_FACTORY_GETTER_H_
#define CONTENT_BROWSER_URL_LOADER_FACTORY_GETTER_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/macros.h"
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

  // Initializes this object on the UI thread. The |partition| is used to
  // initialize the URLLoaderFactories for the network service, AppCache, and
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

  // Like above, except it returns a URLLoaderFactory that has CORB enabled. Use
  // this when using the factory for requests on behalf of a renderer.
  // TODO(lukasza): https://crbug.com/871827: Ensure that |request_initiator| is
  // trustworthy, even when starting requests on behalf of a renderer.
  CONTENT_EXPORT scoped_refptr<network::SharedURLLoaderFactory>
  GetNetworkFactoryWithCORBEnabled();

  // Called on the UI thread to get an info that holds a reference to this
  // URLLoaderFactoryGetter, which can be used to construct a similar
  // SharedURLLoaderFactory as returned from |GetNetworkFactory()| on IO thread.
  CONTENT_EXPORT std::unique_ptr<network::PendingSharedURLLoaderFactory>
  GetPendingNetworkFactory();

  // Called on the IO thread. The factory obtained from here can only be used
  // from the browser process. It must NOT be sent to a renderer process. It has
  // CORB disabled, so it must NOT be used to make requests on behalf of a
  // renderer.
  //
  // When NetworkService is enabled, this clones the internal factory to the
  // network service, which doesn't support auto-reconnect after crash. Useful
  // for one-off requests (e.g. a single navigation) to avoid an additional Mojo
  // hop.
  //
  // When NetworkService is disabled, this clones the non-NetworkService direct
  // network factory.
  CONTENT_EXPORT void CloneNetworkFactory(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory>
          network_factory_receiver);

  // Overrides the network URLLoaderFactory for subsequent requests. Passing a
  // null pointer will restore the default behavior.
  CONTENT_EXPORT void SetNetworkFactoryForTesting(
      network::mojom::URLLoaderFactory* test_factory,
      bool is_corb_enabled = false);

  CONTENT_EXPORT mojo::Remote<network::mojom::URLLoaderFactory>*
  original_network_factory_for_testing() {
    return &network_factory_;
  }

  CONTENT_EXPORT mojo::Remote<network::mojom::URLLoaderFactory>*
  original_network_factory__corb_enabled_for_testing() {
    return &network_factory_corb_enabled_;
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

  // Moves |network_factory| to |network_factory_| or
  // |network_factory_corb_enabled_| depending on |is_corb_enabled| and sets up
  // an error handler.
  void ReinitializeOnIOThread(
      mojo::Remote<network::mojom::URLLoaderFactory> network_factory,
      bool is_corb_enabled);

  // Send |network_factory_request| to cached |StoragePartitionImpl|.
  void HandleNetworkFactoryRequestOnUIThread(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory>
          network_factory_receiver,
      bool is_corb_enabled);

  // Called on the IO thread to get the URLLoaderFactory to the network service.
  // The pointer shouldn't be cached.
  network::mojom::URLLoaderFactory* GetURLLoaderFactory(bool is_corb_enabled);

  // Call |network_factory_.FlushForTesting()|. For test use only. When the
  // flush is complete, |callback| will be called.
  void FlushNetworkInterfaceForTesting(base::OnceClosure callback);

  // Only accessed on IO thread.
  mojo::Remote<network::mojom::URLLoaderFactory> network_factory_;
  mojo::Remote<network::mojom::URLLoaderFactory> network_factory_corb_enabled_;
  network::mojom::URLLoaderFactory* test_factory_ = nullptr;
  network::mojom::URLLoaderFactory* test_factory_corb_enabled_ = nullptr;

  // Used to re-create |network_factory_| when connection error happens. Can
  // only be accessed on UI thread. Must be cleared by |StoragePartitionImpl|
  // when it's going away.
  StoragePartitionImpl* partition_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(URLLoaderFactoryGetter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_URL_LOADER_FACTORY_GETTER_H_
