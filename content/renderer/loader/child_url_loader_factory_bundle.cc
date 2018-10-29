// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/loader/child_url_loader_factory_bundle.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "content/public/common/resource_type.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace content {

namespace {

class URLLoaderRelay : public network::mojom::URLLoaderClient,
                       public network::mojom::URLLoader {
 public:
  URLLoaderRelay(network::mojom::URLLoaderPtr loader_sink,
                 network::mojom::URLLoaderClientRequest client_source,
                 network::mojom::URLLoaderClientPtr client_sink)
      : loader_sink_(std::move(loader_sink)),
        client_source_binding_(this, std::move(client_source)),
        client_sink_(std::move(client_sink)) {}

  // network::mojom::URLLoader implementation:
  void FollowRedirect(const base::Optional<std::vector<std::string>>&
                          to_be_removed_request_headers,
                      const base::Optional<net::HttpRequestHeaders>&
                          modified_request_headers) override {
    DCHECK(!modified_request_headers.has_value())
        << "Redirect with modified headers was not supported yet. "
           "crbug.com/845683";
    loader_sink_->FollowRedirect(base::nullopt, base::nullopt);
  }

  void ProceedWithResponse() override { loader_sink_->ProceedWithResponse(); }

  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override {
    loader_sink_->SetPriority(priority, intra_priority_value);
  }

  void PauseReadingBodyFromNet() override {
    loader_sink_->PauseReadingBodyFromNet();
  }

  void ResumeReadingBodyFromNet() override {
    loader_sink_->ResumeReadingBodyFromNet();
  }

  // network::mojom::URLLoaderClient implementation:
  void OnReceiveResponse(const network::ResourceResponseHead& head) override {
    client_sink_->OnReceiveResponse(head);
  }

  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         const network::ResourceResponseHead& head) override {
    client_sink_->OnReceiveRedirect(redirect_info, head);
  }

  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback callback) override {
    client_sink_->OnUploadProgress(current_position, total_size,
                                   std::move(callback));
  }

  void OnReceiveCachedMetadata(const std::vector<uint8_t>& data) override {
    client_sink_->OnReceiveCachedMetadata(data);
  }

  void OnTransferSizeUpdated(int32_t transfer_size_diff) override {
    client_sink_->OnTransferSizeUpdated(transfer_size_diff);
  }

  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override {
    client_sink_->OnStartLoadingResponseBody(std::move(body));
  }

  void OnComplete(const network::URLLoaderCompletionStatus& status) override {
    client_sink_->OnComplete(status);
  }

 private:
  network::mojom::URLLoaderPtr loader_sink_;
  mojo::Binding<network::mojom::URLLoaderClient> client_source_binding_;
  network::mojom::URLLoaderClientPtr client_sink_;
};

template <typename TKey>
static std::map<TKey, network::mojom::URLLoaderFactoryPtrInfo>
PassInterfacePtrMapToPtrInfoMap(
    std::map<TKey, network::mojom::URLLoaderFactoryPtr> input) {
  std::map<TKey, network::mojom::URLLoaderFactoryPtrInfo> output;
  for (auto& it : input) {
    const TKey& key = it.first;
    network::mojom::URLLoaderFactoryPtr& factory = it.second;
    output.emplace(key, factory.PassInterface());
  }
  return output;
}

}  // namespace

ChildURLLoaderFactoryBundleInfo::ChildURLLoaderFactoryBundleInfo() = default;

ChildURLLoaderFactoryBundleInfo::ChildURLLoaderFactoryBundleInfo(
    std::unique_ptr<URLLoaderFactoryBundleInfo> base_info)
    : URLLoaderFactoryBundleInfo(
          std::move(base_info->default_factory_info()),
          std::move(base_info->scheme_specific_factory_infos()),
          std::move(base_info->initiator_specific_factory_infos()),
          base_info->bypass_redirect_checks()) {}

ChildURLLoaderFactoryBundleInfo::ChildURLLoaderFactoryBundleInfo(
    network::mojom::URLLoaderFactoryPtrInfo default_factory_info,
    SchemeMap scheme_specific_factory_infos,
    OriginMap initiator_specific_factory_infos,
    PossiblyAssociatedURLLoaderFactoryPtrInfo direct_network_factory_info,
    network::mojom::URLLoaderFactoryPtrInfo prefetch_loader_factory_info,
    bool bypass_redirect_checks)
    : URLLoaderFactoryBundleInfo(std::move(default_factory_info),
                                 std::move(scheme_specific_factory_infos),
                                 std::move(initiator_specific_factory_infos),
                                 bypass_redirect_checks),
      direct_network_factory_info_(std::move(direct_network_factory_info)),
      prefetch_loader_factory_info_(std::move(prefetch_loader_factory_info)) {}

ChildURLLoaderFactoryBundleInfo::~ChildURLLoaderFactoryBundleInfo() = default;

scoped_refptr<network::SharedURLLoaderFactory>
ChildURLLoaderFactoryBundleInfo::CreateFactory() {
  auto other = std::make_unique<ChildURLLoaderFactoryBundleInfo>();
  other->default_factory_info_ = std::move(default_factory_info_);
  other->scheme_specific_factory_infos_ =
      std::move(scheme_specific_factory_infos_);
  other->initiator_specific_factory_infos_ =
      std::move(initiator_specific_factory_infos_);
  other->direct_network_factory_info_ = std::move(direct_network_factory_info_);
  other->prefetch_loader_factory_info_ =
      std::move(prefetch_loader_factory_info_);
  other->bypass_redirect_checks_ = bypass_redirect_checks_;

  return base::MakeRefCounted<ChildURLLoaderFactoryBundle>(std::move(other));
}

// -----------------------------------------------------------------------------

ChildURLLoaderFactoryBundle::ChildURLLoaderFactoryBundle() = default;

ChildURLLoaderFactoryBundle::ChildURLLoaderFactoryBundle(
    std::unique_ptr<ChildURLLoaderFactoryBundleInfo> info) {
  Update(std::move(info), base::nullopt);
}

ChildURLLoaderFactoryBundle::ChildURLLoaderFactoryBundle(
    PossiblyAssociatedFactoryGetterCallback direct_network_factory_getter)
    : direct_network_factory_getter_(std::move(direct_network_factory_getter)) {
}

ChildURLLoaderFactoryBundle::~ChildURLLoaderFactoryBundle() = default;

network::mojom::URLLoaderFactory* ChildURLLoaderFactoryBundle::GetFactory(
    const network::ResourceRequest& request) {
  network::mojom::URLLoaderFactory* base_result =
      URLLoaderFactoryBundle::GetFactory(request);
  if (base_result)
    return base_result;

  InitDirectNetworkFactoryIfNecessary();
  DCHECK(direct_network_factory_);
  return direct_network_factory_.get();
}

void ChildURLLoaderFactoryBundle::CreateLoaderAndStart(
    network::mojom::URLLoaderRequest loader,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& request,
    network::mojom::URLLoaderClientPtr client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  auto override_iter = subresource_overrides_.find(request.url);
  if (override_iter != subresource_overrides_.end()) {
    mojom::TransferrableURLLoaderPtr transferrable_loader =
        std::move(override_iter->second);
    subresource_overrides_.erase(override_iter);

    client->OnReceiveResponse(transferrable_loader->head);
    mojo::MakeStrongBinding(
        std::make_unique<URLLoaderRelay>(
            network::mojom::URLLoaderPtr(
                std::move(transferrable_loader->url_loader)),
            std::move(transferrable_loader->url_loader_client),
            std::move(client)),
        std::move(loader));

    return;
  }

  // Use |prefetch_loader_factory_| for prefetch requests to send the requests
  // to the PrefetchURLLoaderService in the browser process and triger the
  // special prefetch handling.
  // TODO(horo): Move this routing logic to network service, when we will have
  // the special prefetch handling in network service.
  if ((request.resource_type == RESOURCE_TYPE_PREFETCH) &&
      prefetch_loader_factory_) {
    prefetch_loader_factory_->CreateLoaderAndStart(
        std::move(loader), routing_id, request_id, options, request,
        std::move(client), traffic_annotation);
    return;
  }

  URLLoaderFactoryBundle::CreateLoaderAndStart(
      std::move(loader), routing_id, request_id, options, request,
      std::move(client), traffic_annotation);
}

std::unique_ptr<network::SharedURLLoaderFactoryInfo>
ChildURLLoaderFactoryBundle::Clone() {
  return CloneInternal(true /* include_default */);
}

std::unique_ptr<network::SharedURLLoaderFactoryInfo>
ChildURLLoaderFactoryBundle::CloneWithoutDefaultFactory() {
  return CloneInternal(false /* include_default */);
}

void ChildURLLoaderFactoryBundle::Update(
    std::unique_ptr<ChildURLLoaderFactoryBundleInfo> info,
    base::Optional<std::vector<mojom::TransferrableURLLoaderPtr>>
        subresource_overrides) {
  if (info->direct_network_factory_info()) {
    direct_network_factory_.Bind(
        std::move(info->direct_network_factory_info()));
  }
  if (info->prefetch_loader_factory_info()) {
    prefetch_loader_factory_.Bind(
        std::move(info->prefetch_loader_factory_info()));
  }
  URLLoaderFactoryBundle::Update(std::move(info));

  if (subresource_overrides) {
    for (auto& element : *subresource_overrides) {
      subresource_overrides_[element->url] = std::move(element);
    }
  }
}

void ChildURLLoaderFactoryBundle::SetPrefetchLoaderFactory(
    network::mojom::URLLoaderFactoryPtr prefetch_loader_factory) {
  prefetch_loader_factory_ = std::move(prefetch_loader_factory);
}

bool ChildURLLoaderFactoryBundle::IsHostChildURLLoaderFactoryBundle() const {
  return false;
}

void ChildURLLoaderFactoryBundle::InitDirectNetworkFactoryIfNecessary() {
  if (direct_network_factory_getter_.is_null())
    return;

  if (!direct_network_factory_) {
    direct_network_factory_ = std::move(direct_network_factory_getter_).Run();
  } else {
    direct_network_factory_getter_.Reset();
  }
}

std::unique_ptr<network::SharedURLLoaderFactoryInfo>
ChildURLLoaderFactoryBundle::CloneInternal(bool include_default) {
  InitDirectNetworkFactoryIfNecessary();

  network::mojom::URLLoaderFactoryPtrInfo default_factory_info;
  if (include_default && default_factory_)
    default_factory_->Clone(mojo::MakeRequest(&default_factory_info));

  network::mojom::URLLoaderFactoryPtrInfo direct_network_factory_info;
  if (direct_network_factory_) {
    direct_network_factory_->Clone(
        mojo::MakeRequest(&direct_network_factory_info));
  }

  network::mojom::URLLoaderFactoryPtrInfo prefetch_loader_factory_info;
  if (prefetch_loader_factory_) {
    prefetch_loader_factory_->Clone(
        mojo::MakeRequest(&prefetch_loader_factory_info));
  }

  // Currently there is no need to override subresources from workers,
  // therefore |subresource_overrides| are not shared with the clones.

  return std::make_unique<ChildURLLoaderFactoryBundleInfo>(
      std::move(default_factory_info),
      ClonePtrMapToPtrInfoMap(scheme_specific_factories_),
      ClonePtrMapToPtrInfoMap(initiator_specific_factories_),
      std::move(direct_network_factory_info),
      std::move(prefetch_loader_factory_info), bypass_redirect_checks_);
}

std::unique_ptr<ChildURLLoaderFactoryBundleInfo>
ChildURLLoaderFactoryBundle::PassInterface() {
  InitDirectNetworkFactoryIfNecessary();

  network::mojom::URLLoaderFactoryPtrInfo default_factory_info;
  if (default_factory_)
    default_factory_info = default_factory_.PassInterface();

  PossiblyAssociatedInterfacePtrInfo<network::mojom::URLLoaderFactory>
      direct_network_factory_info;
  if (direct_network_factory_) {
    direct_network_factory_info = direct_network_factory_.PassInterface();
  }

  network::mojom::URLLoaderFactoryPtrInfo prefetch_loader_factory_info;
  if (prefetch_loader_factory_) {
    prefetch_loader_factory_info = prefetch_loader_factory_.PassInterface();
  }

  return std::make_unique<ChildURLLoaderFactoryBundleInfo>(
      std::move(default_factory_info),
      PassInterfacePtrMapToPtrInfoMap(std::move(scheme_specific_factories_)),
      PassInterfacePtrMapToPtrInfoMap(std::move(initiator_specific_factories_)),
      std::move(direct_network_factory_info),
      std::move(prefetch_loader_factory_info), bypass_redirect_checks_);
}

}  // namespace content
