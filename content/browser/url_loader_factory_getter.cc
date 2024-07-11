// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/url_loader_factory_getter.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/lazy_instance.h"
#include "base/run_loop.h"
#include "content/browser/loader/url_loader_factory_utils.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_thread.h"
#include "services/network/public/cpp/cross_thread_pending_shared_url_loader_factory.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"

namespace content {

ReconnectableURLLoaderFactory::ReconnectableURLLoaderFactory(
    CreateCallback create_url_loader_factory_callback)
    : create_url_loader_factory_callback_(
          std::move(create_url_loader_factory_callback)) {}

ReconnectableURLLoaderFactory::~ReconnectableURLLoaderFactory() = default;

void ReconnectableURLLoaderFactory::CreateLoaderAndStart(
    mojo::PendingReceiver<network::mojom::URLLoader> receiver,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& url_request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  if (network::mojom::URLLoaderFactory* factory = GetURLLoaderFactory()) {
    factory->CreateLoaderAndStart(std::move(receiver), request_id, options,
                                  url_request, std::move(client),
                                  traffic_annotation);
  }
}

void ReconnectableURLLoaderFactory::Clone(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver) {
  if (network::mojom::URLLoaderFactory* factory = GetURLLoaderFactory()) {
    factory->Clone(std::move(receiver));
  }
}

std::unique_ptr<network::PendingSharedURLLoaderFactory>
ReconnectableURLLoaderFactory::Clone() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::make_unique<network::CrossThreadPendingSharedURLLoaderFactory>(
      this);
}

network::mojom::URLLoaderFactory*
ReconnectableURLLoaderFactory::GetURLLoaderFactory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Create the URLLoaderFactory as needed, but make sure not to reuse a
  // previously created one if the test override has changed.
  if (url_loader_factory_ && url_loader_factory_.is_connected() &&
      is_test_url_loader_factory_ ==
          !!url_loader_factory::GetTestingInterceptor()) {
    return url_loader_factory_.get();
  }

  is_test_url_loader_factory_ = !!url_loader_factory::GetTestingInterceptor();
  url_loader_factory_.reset();

  mojo::PendingRemote<network::mojom::URLLoaderFactory> url_loader_factory;
  create_url_loader_factory_callback_.Run(&url_loader_factory);
  if (!url_loader_factory) {
    return nullptr;
  }

  url_loader_factory_.Bind(std::move(url_loader_factory));
  return url_loader_factory_.get();
}

void ReconnectableURLLoaderFactory::Reset() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  url_loader_factory_.reset();
}

void ReconnectableURLLoaderFactory::FlushForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (url_loader_factory_) {
    url_loader_factory_.FlushForTesting();  // IN-TEST
  }
}

// -----------------------------------------------------------------------------

namespace {
base::LazyInstance<URLLoaderFactoryGetter::GetNetworkFactoryCallback>::Leaky
    g_get_network_factory_callback = LAZY_INSTANCE_INITIALIZER;
}

class URLLoaderFactoryGetter::PendingURLLoaderFactoryForIOThread
    : public network::PendingSharedURLLoaderFactory {
 public:
  PendingURLLoaderFactoryForIOThread() = default;
  explicit PendingURLLoaderFactoryForIOThread(
      scoped_refptr<URLLoaderFactoryGetter> factory_getter)
      : factory_getter_(std::move(factory_getter)) {}

  PendingURLLoaderFactoryForIOThread(
      const PendingURLLoaderFactoryForIOThread&) = delete;
  PendingURLLoaderFactoryForIOThread& operator=(
      const PendingURLLoaderFactoryForIOThread&) = delete;

  ~PendingURLLoaderFactoryForIOThread() override = default;

  scoped_refptr<URLLoaderFactoryGetter>& url_loader_factory_getter() {
    return factory_getter_;
  }

 protected:
  // PendingSharedURLLoaderFactory implementation.
  scoped_refptr<network::SharedURLLoaderFactory> CreateFactory() override;

  scoped_refptr<URLLoaderFactoryGetter> factory_getter_;
};

class URLLoaderFactoryGetter::URLLoaderFactoryForIOThread
    : public network::SharedURLLoaderFactory {
 public:
  URLLoaderFactoryForIOThread(
      scoped_refptr<URLLoaderFactoryGetter> factory_getter)
      : factory_getter_(std::move(factory_getter)) {
    DCHECK(!BrowserThread::IsThreadInitialized(BrowserThread::IO) ||
           BrowserThread::CurrentlyOn(BrowserThread::IO));
  }

  explicit URLLoaderFactoryForIOThread(
      std::unique_ptr<PendingURLLoaderFactoryForIOThread> info)
      : factory_getter_(std::move(info->url_loader_factory_getter())) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
  }

  URLLoaderFactoryForIOThread(const URLLoaderFactoryForIOThread&) = delete;
  URLLoaderFactoryForIOThread& operator=(const URLLoaderFactoryForIOThread&) =
      delete;

  // mojom::URLLoaderFactory implementation:
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& url_request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    if (!factory_getter_)
      return;
    factory_getter_->GetURLLoaderFactory()->CreateLoaderAndStart(
        std::move(receiver), request_id, options, url_request,
        std::move(client), traffic_annotation);
  }

  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver)
      override {
    if (!factory_getter_)
      return;
    factory_getter_->GetURLLoaderFactory()->Clone(std::move(receiver));
  }

  // SharedURLLoaderFactory implementation:
  std::unique_ptr<network::PendingSharedURLLoaderFactory> Clone() override {
    NOTREACHED_IN_MIGRATION()
        << "This isn't supported. If you need a SharedURLLoaderFactory"
           " on the UI thread, get it from StoragePartition.";
    return nullptr;
  }

 private:
  friend class base::RefCounted<URLLoaderFactoryForIOThread>;
  ~URLLoaderFactoryForIOThread() override = default;

  scoped_refptr<URLLoaderFactoryGetter> factory_getter_;
};

scoped_refptr<network::SharedURLLoaderFactory>
URLLoaderFactoryGetter::PendingURLLoaderFactoryForIOThread::CreateFactory() {
  auto other = std::make_unique<PendingURLLoaderFactoryForIOThread>();
  other->factory_getter_ = std::move(factory_getter_);

  return base::MakeRefCounted<URLLoaderFactoryForIOThread>(std::move(other));
}

// -----------------------------------------------------------------------------

URLLoaderFactoryGetter::URLLoaderFactoryGetter() = default;

void URLLoaderFactoryGetter::Initialize(StoragePartitionImpl* partition) {
  DCHECK(partition);
  partition_ = partition;

  // Create a mojo::PendingRemote<URLLoaderFactory> synchronously and push it to
  // the IO thread. If the pipe errors out later due to a network service crash,
  // the pipe is created on the IO thread, and the request send back to the UI
  // thread.
  // TODO(mmenke):  Is one less thread hop on startup worth the extra complexity
  // of two different pipe creation paths?
  mojo::PendingRemote<network::mojom::URLLoaderFactory> network_factory;
  HandleNetworkFactoryRequestOnUIThread(
      network_factory.InitWithNewPipeAndPassReceiver());

  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&URLLoaderFactoryGetter::InitializeOnIOThread,
                                this, std::move(network_factory)));
}

void URLLoaderFactoryGetter::OnStoragePartitionDestroyed() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  partition_ = nullptr;
}

scoped_refptr<network::SharedURLLoaderFactory>
URLLoaderFactoryGetter::GetNetworkFactory() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return base::MakeRefCounted<URLLoaderFactoryForIOThread>(
      base::WrapRefCounted(this));
}

std::unique_ptr<network::PendingSharedURLLoaderFactory>
URLLoaderFactoryGetter::GetPendingNetworkFactory() {
  return std::make_unique<PendingURLLoaderFactoryForIOThread>(
      base::WrapRefCounted(this));
}

network::mojom::URLLoaderFactory*
URLLoaderFactoryGetter::GetURLLoaderFactory() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // This needs to be done before returning |test_factory_|, as the
  // |test_factory_| may fall back to |network_factory_|. The |is_bound()| check
  // is only needed by unit tests.
  mojo::Remote<network::mojom::URLLoaderFactory>* factory = &network_factory_;
  if (!factory->is_bound() || !factory->is_connected()) {
    mojo::Remote<network::mojom::URLLoaderFactory> network_factory;
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            &URLLoaderFactoryGetter::HandleNetworkFactoryRequestOnUIThread,
            this, network_factory.BindNewPipeAndPassReceiver()));
    ReinitializeOnIOThread(std::move(network_factory));
  }

  if (g_get_network_factory_callback.Get() && !test_factory_)
    g_get_network_factory_callback.Get().Run(this);

  if (test_factory_) {
    return test_factory_;
  }

  return factory->get();
}

void URLLoaderFactoryGetter::SetNetworkFactoryForTesting(
    network::mojom::URLLoaderFactory* test_factory) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!test_factory_ || !test_factory);
  test_factory_ = test_factory;
}

void URLLoaderFactoryGetter::SetGetNetworkFactoryCallbackForTesting(
    const GetNetworkFactoryCallback& get_network_factory_callback) {
  DCHECK(!BrowserThread::IsThreadInitialized(BrowserThread::IO) ||
         BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!g_get_network_factory_callback.Get() ||
         !get_network_factory_callback);
  g_get_network_factory_callback.Get() = get_network_factory_callback;
}

void URLLoaderFactoryGetter::FlushNetworkInterfaceOnIOThreadForTesting() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::RunLoop run_loop;
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&URLLoaderFactoryGetter::FlushNetworkInterfaceForTesting,
                     this, run_loop.QuitClosure()));
  run_loop.Run();
}

void URLLoaderFactoryGetter::FlushNetworkInterfaceForTesting(
    base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (network_factory_)
    network_factory_.FlushAsyncForTesting(std::move(callback));
}

URLLoaderFactoryGetter::~URLLoaderFactoryGetter() {}

void URLLoaderFactoryGetter::InitializeOnIOThread(
    mojo::PendingRemote<network::mojom::URLLoaderFactory> network_factory) {
  ReinitializeOnIOThread(mojo::Remote<network::mojom::URLLoaderFactory>(
      std::move(network_factory)));
}

void URLLoaderFactoryGetter::ReinitializeOnIOThread(
    mojo::Remote<network::mojom::URLLoaderFactory> network_factory) {
  DCHECK(network_factory.is_bound());
  // Set a disconnect handler so that connection errors on the pipes are
  // noticed, but the class doesn't actually do anything when the error is
  // observed - instead, a new pipe is created in GetURLLoaderFactory() as
  // needed. This is to avoid incrementing the reference count of |this| in the
  // callback, as that could result in increasing the reference count from 0 to
  // 1 while there's a pending task to delete |this|. See
  // https://crbug.com/870942 for more details.
  network_factory.set_disconnect_handler(base::DoNothing());
  network_factory_ = std::move(network_factory);
}

void URLLoaderFactoryGetter::HandleNetworkFactoryRequestOnUIThread(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory>
        network_factory_receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // |StoragePartitionImpl| may have went away while |URLLoaderFactoryGetter| is
  // still held by consumers.
  if (!partition_)
    return;
  partition_->GetNetworkContext()->CreateURLLoaderFactory(
      std::move(network_factory_receiver),
      partition_->CreateURLLoaderFactoryParams());
}

}  // namespace content
