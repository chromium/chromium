// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/url_loader_factory_getter.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/lazy_instance.h"
#include "base/task/post_task.h"
#include "content/browser/storage_partition_impl.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/common/content_switches.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/network_service.mojom.h"

namespace content {

namespace {
base::LazyInstance<URLLoaderFactoryGetter::GetNetworkFactoryCallback>::Leaky
    g_get_network_factory_callback = LAZY_INSTANCE_INITIALIZER;
}

class URLLoaderFactoryGetter::URLLoaderFactoryForIOThreadInfo
    : public network::SharedURLLoaderFactoryInfo {
 public:
  URLLoaderFactoryForIOThreadInfo() = default;
  explicit URLLoaderFactoryForIOThreadInfo(
      scoped_refptr<URLLoaderFactoryGetter> factory_getter)
      : factory_getter_(std::move(factory_getter)) {}
  ~URLLoaderFactoryForIOThreadInfo() override = default;

  scoped_refptr<URLLoaderFactoryGetter>& url_loader_factory_getter() {
    return factory_getter_;
  }

 protected:
  // SharedURLLoaderFactoryInfo implementation.
  scoped_refptr<network::SharedURLLoaderFactory> CreateFactory() override;

  scoped_refptr<URLLoaderFactoryGetter> factory_getter_;

  DISALLOW_COPY_AND_ASSIGN(URLLoaderFactoryForIOThreadInfo);
};

class URLLoaderFactoryGetter::URLLoaderFactoryForIOThread
    : public network::SharedURLLoaderFactory {
 public:
  explicit URLLoaderFactoryForIOThread(
      scoped_refptr<URLLoaderFactoryGetter> factory_getter)
      : factory_getter_(std::move(factory_getter)) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
  }

  explicit URLLoaderFactoryForIOThread(
      std::unique_ptr<URLLoaderFactoryForIOThreadInfo> info)
      : factory_getter_(std::move(info->url_loader_factory_getter())) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
  }

  // mojom::URLLoaderFactory implementation:
  void CreateLoaderAndStart(network::mojom::URLLoaderRequest request,
                            int32_t routing_id,
                            int32_t request_id,
                            uint32_t options,
                            const network::ResourceRequest& url_request,
                            network::mojom::URLLoaderClientPtr client,
                            const net::MutableNetworkTrafficAnnotationTag&
                                traffic_annotation) override {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    if (!factory_getter_)
      return;
    factory_getter_->GetURLLoaderFactory()->CreateLoaderAndStart(
        std::move(request), routing_id, request_id, options, url_request,
        std::move(client), traffic_annotation);
  }

  void Clone(network::mojom::URLLoaderFactoryRequest request) override {
    if (!factory_getter_)
      return;
    factory_getter_->GetURLLoaderFactory()->Clone(std::move(request));
  }

  // SharedURLLoaderFactory implementation:
  std::unique_ptr<network::SharedURLLoaderFactoryInfo> Clone() override {
    NOTREACHED() << "This isn't supported. If you need a SharedURLLoaderFactory"
                    " on the UI thread, get it from StoragePartition.";
    return nullptr;
  }

 private:
  friend class base::RefCounted<URLLoaderFactoryForIOThread>;
  ~URLLoaderFactoryForIOThread() override = default;

  scoped_refptr<URLLoaderFactoryGetter> factory_getter_;

  DISALLOW_COPY_AND_ASSIGN(URLLoaderFactoryForIOThread);
};

scoped_refptr<network::SharedURLLoaderFactory>
URLLoaderFactoryGetter::URLLoaderFactoryForIOThreadInfo::CreateFactory() {
  auto other = std::make_unique<URLLoaderFactoryForIOThreadInfo>();
  other->factory_getter_ = std::move(factory_getter_);

  return base::MakeRefCounted<URLLoaderFactoryForIOThread>(std::move(other));
}

// -----------------------------------------------------------------------------

URLLoaderFactoryGetter::URLLoaderFactoryGetter() = default;

void URLLoaderFactoryGetter::Initialize(StoragePartitionImpl* partition) {
  DCHECK(partition);
  partition_ = partition;

  // Create a URLLoaderFactoryPtr synchronously and push it to the IO thread. If
  // the pipe errors out later due to a network service crash, the pipe is
  // created on the IO thread, and the request send back to the UI thread.
  // TODO(mmenke):  Is one less thread hop on startup worth the extra complexity
  // of two different pipe creation paths?
  DCHECK(!pending_network_factory_request_.is_pending());
  network::mojom::URLLoaderFactoryPtr network_factory;
  pending_network_factory_request_ = MakeRequest(&network_factory);

  // If NetworkService is disabled, HandleFactoryRequests should be called after
  // NetworkContext in |partition_| is ready.
  if (base::FeatureList::IsEnabled(network::features::kNetworkService))
    HandleFactoryRequests();

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&URLLoaderFactoryGetter::InitializeOnIOThread, this,
                     network_factory.PassInterface()));
}

void URLLoaderFactoryGetter::HandleFactoryRequests() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(pending_network_factory_request_.is_pending());
  HandleNetworkFactoryRequestOnUIThread(
      std::move(pending_network_factory_request_));
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

std::unique_ptr<network::SharedURLLoaderFactoryInfo>
URLLoaderFactoryGetter::GetNetworkFactoryInfo() {
  return std::make_unique<URLLoaderFactoryForIOThreadInfo>(
      base::WrapRefCounted(this));
}

network::mojom::URLLoaderFactory*
URLLoaderFactoryGetter::GetURLLoaderFactory() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // This needs to be done before returning |test_factory_|, as the
  // |test_factory_| may fall back to |network_factory_|. The |is_bound()| check
  // is only needed by unit tests.
  if (network_factory_.encountered_error() || !network_factory_.is_bound()) {
    network::mojom::URLLoaderFactoryPtr network_factory;
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(
            &URLLoaderFactoryGetter::HandleNetworkFactoryRequestOnUIThread,
            this, mojo::MakeRequest(&network_factory)));
    ReinitializeOnIOThread(std::move(network_factory));
  }

  if (g_get_network_factory_callback.Get() && !test_factory_)
    g_get_network_factory_callback.Get().Run(this);

  if (test_factory_)
    return test_factory_;

  return network_factory_.get();
}

void URLLoaderFactoryGetter::CloneNetworkFactory(
    network::mojom::URLLoaderFactoryRequest network_factory_request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  GetURLLoaderFactory()->Clone(std::move(network_factory_request));
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
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
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
    network::mojom::URLLoaderFactoryPtrInfo network_factory) {
  ReinitializeOnIOThread(
      network::mojom::URLLoaderFactoryPtr(std::move(network_factory)));
}

void URLLoaderFactoryGetter::ReinitializeOnIOThread(
    network::mojom::URLLoaderFactoryPtr network_factory) {
  DCHECK(network_factory.is_bound());
  network_factory_ = std::move(network_factory);
  // Set a connection error handle so that connection errors on the pipes are
  // noticed, but the class doesn't actually do anything when the error is
  // observed - instead, a new pipe is created in GetURLLoaderFactory() as
  // needed. This is to avoid incrementing the reference count of |this| in the
  // callback, as that could result in increasing the reference count from 0 to
  // 1 while there's a pending task to delete |this|. See
  // https://crbug.com/870942 for more details.
  network_factory_.set_connection_error_handler(base::DoNothing());
}

void URLLoaderFactoryGetter::HandleNetworkFactoryRequestOnUIThread(
    network::mojom::URLLoaderFactoryRequest network_factory_request) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // |StoragePartitionImpl| may have went away while |URLLoaderFactoryGetter| is
  // still held by consumers.
  if (!partition_)
    return;
  network::mojom::URLLoaderFactoryParamsPtr params =
      network::mojom::URLLoaderFactoryParams::New();
  params->process_id = network::mojom::kBrowserProcessId;
  params->is_corb_enabled = false;
  params->disable_web_security =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableWebSecurity);
  partition_->GetNetworkContext()->CreateURLLoaderFactory(
      std::move(network_factory_request), std::move(params));
}

}  // namespace content
