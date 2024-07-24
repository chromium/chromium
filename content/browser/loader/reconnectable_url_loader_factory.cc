// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/reconnectable_url_loader_factory.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "content/browser/loader/url_loader_factory_utils.h"
#include "services/network/public/cpp/cross_thread_pending_shared_url_loader_factory.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

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

class ReconnectableURLLoaderFactoryForIOThread::
    PendingURLLoaderFactoryForIOThread final
    : public network::PendingSharedURLLoaderFactory {
 public:
  explicit PendingURLLoaderFactoryForIOThread(
      scoped_refptr<ReconnectableURLLoaderFactoryForIOThread> factory_getter)
      : factory_getter_(std::move(factory_getter)) {
    CHECK(factory_getter_);
  }

  PendingURLLoaderFactoryForIOThread(
      const PendingURLLoaderFactoryForIOThread&) = delete;
  PendingURLLoaderFactoryForIOThread& operator=(
      const PendingURLLoaderFactoryForIOThread&) = delete;

  ~PendingURLLoaderFactoryForIOThread() override = default;

 protected:
  // PendingSharedURLLoaderFactory implementation.
  scoped_refptr<network::SharedURLLoaderFactory> CreateFactory() override;

  const scoped_refptr<ReconnectableURLLoaderFactoryForIOThread> factory_getter_;
};

class ReconnectableURLLoaderFactoryForIOThread::URLLoaderFactoryForIOThread
    final : public network::SharedURLLoaderFactory {
 public:
  explicit URLLoaderFactoryForIOThread(
      scoped_refptr<ReconnectableURLLoaderFactoryForIOThread> factory_getter)
      : factory_getter_(std::move(factory_getter)) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    CHECK(factory_getter_);
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
    factory_getter_->GetURLLoaderFactory()->CreateLoaderAndStart(
        std::move(receiver), request_id, options, url_request,
        std::move(client), traffic_annotation);
  }

  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver)
      override {
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

  const scoped_refptr<ReconnectableURLLoaderFactoryForIOThread> factory_getter_;
};

scoped_refptr<network::SharedURLLoaderFactory>
ReconnectableURLLoaderFactoryForIOThread::PendingURLLoaderFactoryForIOThread::
    CreateFactory() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return base::MakeRefCounted<URLLoaderFactoryForIOThread>(
      std::move(factory_getter_));
}

// -----------------------------------------------------------------------------

ReconnectableURLLoaderFactoryForIOThread::
    ReconnectableURLLoaderFactoryForIOThread(
        CreateCallback create_url_loader_factory_callback)
    : create_url_loader_factory_callback_(
          std::move(create_url_loader_factory_callback)) {}

void ReconnectableURLLoaderFactoryForIOThread::Initialize() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

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
      FROM_HERE,
      base::BindOnce(
          &ReconnectableURLLoaderFactoryForIOThread::InitializeOnIOThread, this,
          std::move(network_factory)));
}

std::unique_ptr<network::PendingSharedURLLoaderFactory>
ReconnectableURLLoaderFactoryForIOThread::CloneForIOThread() {
  return std::make_unique<PendingURLLoaderFactoryForIOThread>(
      base::WrapRefCounted(this));
}

network::mojom::URLLoaderFactory*
ReconnectableURLLoaderFactoryForIOThread::GetURLLoaderFactory() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (url_loader_factory_ && url_loader_factory_.is_connected() &&
      is_test_url_loader_factory_ ==
          url_loader_factory::HasInterceptorOnIOThreadForTesting()) {
    return url_loader_factory_.get();
  }

  is_test_url_loader_factory_ =
      url_loader_factory::HasInterceptorOnIOThreadForTesting();
  mojo::Remote<network::mojom::URLLoaderFactory> network_factory;
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&ReconnectableURLLoaderFactoryForIOThread::
                         HandleNetworkFactoryRequestOnUIThread,
                     this, network_factory.BindNewPipeAndPassReceiver()));
  ReinitializeOnIOThread(std::move(network_factory));
  return url_loader_factory_.get();
}

void ReconnectableURLLoaderFactoryForIOThread::Reset() {
  Initialize();
}

void ReconnectableURLLoaderFactoryForIOThread::FlushForTesting() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::RunLoop run_loop;
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &ReconnectableURLLoaderFactoryForIOThread::FlushOnIOThreadForTesting,
          this, run_loop.QuitClosure()));
  run_loop.Run();
}

void ReconnectableURLLoaderFactoryForIOThread::FlushOnIOThreadForTesting(
    base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (url_loader_factory_) {
    url_loader_factory_.FlushAsyncForTesting(std::move(callback));  // IN-TEST
  }
}

ReconnectableURLLoaderFactoryForIOThread::
    ~ReconnectableURLLoaderFactoryForIOThread() = default;

void ReconnectableURLLoaderFactoryForIOThread::InitializeOnIOThread(
    mojo::PendingRemote<network::mojom::URLLoaderFactory> network_factory) {
  ReinitializeOnIOThread(mojo::Remote<network::mojom::URLLoaderFactory>(
      std::move(network_factory)));
}

void ReconnectableURLLoaderFactoryForIOThread::ReinitializeOnIOThread(
    mojo::Remote<network::mojom::URLLoaderFactory> network_factory) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(network_factory.is_bound());
  // Set a disconnect handler so that connection errors on the pipes are
  // noticed, but the class doesn't actually do anything when the error is
  // observed - instead, a new pipe is created in GetURLLoaderFactory() as
  // needed. This is to avoid incrementing the reference count of |this| in the
  // callback, as that could result in increasing the reference count from 0 to
  // 1 while there's a pending task to delete |this|. See
  // https://crbug.com/870942 for more details.
  network_factory.set_disconnect_handler(base::DoNothing());
  url_loader_factory_ = std::move(network_factory);
}

void ReconnectableURLLoaderFactoryForIOThread::
    HandleNetworkFactoryRequestOnUIThread(
        mojo::PendingReceiver<network::mojom::URLLoaderFactory>
            network_factory_receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  mojo::PendingRemote<network::mojom::URLLoaderFactory> factory_remote;
  create_url_loader_factory_callback_.Run(&factory_remote);

  if (!factory_remote) {
    // The underlying URLLoaderFactory has went away while
    // `ReconnectableURLLoaderFactoryForIOThread` is still held by consumers.
    return;
  }

  CHECK(mojo::FusePipes(std::move(network_factory_receiver),
                        std::move(factory_remote)));
}

// -----------------------------------------------------------------------------

ReconnectableURLLoaderFactoryForIOThreadWrapper::
    ReconnectableURLLoaderFactoryForIOThreadWrapper(
        CreateCallback create_url_loader_factory_callback)
    : factory_(base::MakeRefCounted<ReconnectableURLLoaderFactory>(
          create_url_loader_factory_callback)),
      factory_for_io_thread_(
          base::MakeRefCounted<ReconnectableURLLoaderFactoryForIOThread>(
              create_url_loader_factory_callback)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

ReconnectableURLLoaderFactoryForIOThreadWrapper::
    ~ReconnectableURLLoaderFactoryForIOThreadWrapper() = default;

}  // namespace content
