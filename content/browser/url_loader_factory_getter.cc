// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/url_loader_factory_getter.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/lazy_instance.h"
#include "base/run_loop.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/network_service.mojom.h"

namespace content {

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
      scoped_refptr<URLLoaderFactoryGetter> factory_getter,
      bool is_corb_enabled)
      : factory_getter_(std::move(factory_getter)),
        is_corb_enabled_(is_corb_enabled) {
    DCHECK(!BrowserThread::IsThreadInitialized(BrowserThread::IO) ||
           BrowserThread::CurrentlyOn(BrowserThread::IO));
  }

  explicit URLLoaderFactoryForIOThread(
      std::unique_ptr<PendingURLLoaderFactoryForIOThread> info)
      : factory_getter_(std::move(info->url_loader_factory_getter())),
        is_corb_enabled_(false) {
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
    factory_getter_->GetURLLoaderFactory(is_corb_enabled_)
        ->CreateLoaderAndStart(std::move(receiver), request_id, options,
                               url_request, std::move(client),
                               traffic_annotation);
  }

  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver)
      override {
    if (!factory_getter_)
      return;
    factory_getter_->GetURLLoaderFactory(is_corb_enabled_)
        ->Clone(std::move(receiver));
  }

  // SharedURLLoaderFactory implementation:
  std::unique_ptr<network::PendingSharedURLLoaderFactory> Clone() override {
    NOTREACHED() << "This isn't supported. If you need a SharedURLLoaderFactory"
                    " on the UI thread, get it from StoragePartition.";
    return nullptr;
  }

 private:
  friend class base::RefCounted<URLLoaderFactoryForIOThread>;
  ~URLLoaderFactoryForIOThread() override = default;

  scoped_refptr<URLLoaderFactoryGetter> factory_getter_;
  bool is_corb_enabled_;
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
      network_factory.InitWithNewPipeAndPassReceiver(), false);

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
      base::WrapRefCounted(this), false);
}

scoped_refptr<network::SharedURLLoaderFactory>
URLLoaderFactoryGetter::GetNetworkFactoryWithCORBEnabled() {
  DCHECK(!BrowserThread::IsThreadInitialized(BrowserThread::IO) ||
         BrowserThread::CurrentlyOn(BrowserThread::IO));
  return base::MakeRefCounted<URLLoaderFactoryForIOThread>(
      base::WrapRefCounted(this), true);
}

std::unique_ptr<network::PendingSharedURLLoaderFactory>
URLLoaderFactoryGetter::GetPendingNetworkFactory() {
  return std::make_unique<PendingURLLoaderFactoryForIOThread>(
      base::WrapRefCounted(this));
}

network::mojom::URLLoaderFactory* URLLoaderFactoryGetter::GetURLLoaderFactory(
    bool is_corb_enabled) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // This needs to be done before returning |test_factory_|, as the
  // |test_factory_| may fall back to |network_factory_|. The |is_bound()| check
  // is only needed by unit tests.
  mojo::Remote<network::mojom::URLLoaderFactory>* factory =
      is_corb_enabled ? &network_factory_corb_enabled_ : &network_factory_;
  if (!factory->is_bound() || !factory->is_connected()) {
    mojo::Remote<network::mojom::URLLoaderFactory> network_factory;
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            &URLLoaderFactoryGetter::HandleNetworkFactoryRequestOnUIThread,
            this, network_factory.BindNewPipeAndPassReceiver(),
            is_corb_enabled));
    ReinitializeOnIOThread(std::move(network_factory), is_corb_enabled);
  }

  if (g_get_network_factory_callback.Get() && !test_factory_)
    g_get_network_factory_callback.Get().Run(this);

  if (is_corb_enabled && test_factory_corb_enabled_)
    return test_factory_corb_enabled_;

  if (!is_corb_enabled && test_factory_)
    return test_factory_;

  return factory->get();
}

void URLLoaderFactoryGetter::CloneNetworkFactory(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory>
        network_factory_receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  GetURLLoaderFactory(false)->Clone(std::move(network_factory_receiver));
}

void URLLoaderFactoryGetter::SetNetworkFactoryForTesting(
    network::mojom::URLLoaderFactory* test_factory,
    bool is_corb_enabled) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (is_corb_enabled) {
    DCHECK(!test_factory_corb_enabled_ || !test_factory);
    test_factory_corb_enabled_ = test_factory;
  } else {
    DCHECK(!test_factory_ || !test_factory);
    test_factory_ = test_factory;
  }
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
  if (network_factory_corb_enabled_)
    network_factory_corb_enabled_.FlushAsyncForTesting(std::move(callback));
}

URLLoaderFactoryGetter::~URLLoaderFactoryGetter() {}

void URLLoaderFactoryGetter::InitializeOnIOThread(
    mojo::PendingRemote<network::mojom::URLLoaderFactory> network_factory) {
  ReinitializeOnIOThread(mojo::Remote<network::mojom::URLLoaderFactory>(
                             std::move(network_factory)),
                         false);
}

void URLLoaderFactoryGetter::ReinitializeOnIOThread(
    mojo::Remote<network::mojom::URLLoaderFactory> network_factory,
    bool is_corb_enabled) {
  DCHECK(network_factory.is_bound());
  // Set a disconnect handler so that connection errors on the pipes are
  // noticed, but the class doesn't actually do anything when the error is
  // observed - instead, a new pipe is created in GetURLLoaderFactory() as
  // needed. This is to avoid incrementing the reference count of |this| in the
  // callback, as that could result in increasing the reference count from 0 to
  // 1 while there's a pending task to delete |this|. See
  // https://crbug.com/870942 for more details.
  network_factory.set_disconnect_handler(base::DoNothing());
  if (is_corb_enabled) {
    network_factory_corb_enabled_ = std::move(network_factory);
  } else {
    network_factory_ = std::move(network_factory);
  }
}

void URLLoaderFactoryGetter::HandleNetworkFactoryRequestOnUIThread(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory>
        network_factory_receiver,
    bool is_corb_enabled) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // |StoragePartitionImpl| may have went away while |URLLoaderFactoryGetter| is
  // still held by consumers.
  if (!partition_)
    return;
  network::mojom::URLLoaderFactoryParamsPtr params =
      network::mojom::URLLoaderFactoryParams::New();
  // The browser process is considered trusted.
  params->is_trusted = true;
  params->process_id = network::mojom::kBrowserProcessId;
  params->automatically_assign_isolation_info = true;
  params->is_corb_enabled = is_corb_enabled;
  params->disable_web_security =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableWebSecurity);
  partition_->GetNetworkContext()->CreateURLLoaderFactory(
      std::move(network_factory_receiver), std::move(params));
}

}  // namespace content
