// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_URL_LOADER_FACTORY_BUNDLE_H_
#define CONTENT_COMMON_URL_LOADER_FACTORY_BUNDLE_H_

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/macros.h"
#include "content/common/content_export.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "url/origin.h"

namespace network {
struct ResourceRequest;
};

namespace content {

// Holds the internal state of a URLLoaderFactoryBundle in a form that is safe
// to pass across sequences.
class CONTENT_EXPORT URLLoaderFactoryBundleInfo
    : public network::SharedURLLoaderFactoryInfo {
 public:
  // Map from URL scheme to URLLoaderFactoryPtrInfo for handling URL requests
  // for schemes not handled by the |default_factory_info|. See also
  // URLLoaderFactoryBundle::SchemeMap.
  using SchemeMap =
      std::map<std::string, network::mojom::URLLoaderFactoryPtrInfo>;

  // Map from origin of request initiator to URLLoaderFactoryPtrInfo for
  // handling this initiator's requests (e.g. for relaxing CORB for requests
  // initiated from content scripts).
  using OriginMap =
      std::map<url::Origin, network::mojom::URLLoaderFactoryPtrInfo>;

  URLLoaderFactoryBundleInfo();
  URLLoaderFactoryBundleInfo(
      network::mojom::URLLoaderFactoryPtrInfo default_factory_info,
      SchemeMap scheme_specific_factory_infos,
      OriginMap initiator_specific_factory_infos,
      bool bypass_redirect_checks);
  ~URLLoaderFactoryBundleInfo() override;

  network::mojom::URLLoaderFactoryPtrInfo& default_factory_info() {
    return default_factory_info_;
  }

  SchemeMap& scheme_specific_factory_infos() {
    return scheme_specific_factory_infos_;
  }
  OriginMap& initiator_specific_factory_infos() {
    return initiator_specific_factory_infos_;
  }

  bool bypass_redirect_checks() const { return bypass_redirect_checks_; }
  void set_bypass_redirect_checks(bool bypass_redirect_checks) {
    bypass_redirect_checks_ = bypass_redirect_checks;
  }

 protected:
  // SharedURLLoaderFactoryInfo implementation.
  scoped_refptr<network::SharedURLLoaderFactory> CreateFactory() override;

  network::mojom::URLLoaderFactoryPtrInfo default_factory_info_;
  SchemeMap scheme_specific_factory_infos_;
  OriginMap initiator_specific_factory_infos_;
  bool bypass_redirect_checks_ = false;

  DISALLOW_COPY_AND_ASSIGN(URLLoaderFactoryBundleInfo);
};

// Encapsulates a collection of URLLoaderFactoryPtrs which can be usd to acquire
// loaders for various types of resource requests.
class CONTENT_EXPORT URLLoaderFactoryBundle
    : public network::SharedURLLoaderFactory {
 public:
  URLLoaderFactoryBundle();

  explicit URLLoaderFactoryBundle(
      std::unique_ptr<URLLoaderFactoryBundleInfo> info);

  // Sets the default factory to use when no registered factories match a given
  // |url|.
  void SetDefaultFactory(network::mojom::URLLoaderFactoryPtr factory);

  // SharedURLLoaderFactory implementation.
  void CreateLoaderAndStart(network::mojom::URLLoaderRequest loader,
                            int32_t routing_id,
                            int32_t request_id,
                            uint32_t options,
                            const network::ResourceRequest& request,
                            network::mojom::URLLoaderClientPtr client,
                            const net::MutableNetworkTrafficAnnotationTag&
                                traffic_annotation) override;
  void Clone(network::mojom::URLLoaderFactoryRequest request) override;
  std::unique_ptr<network::SharedURLLoaderFactoryInfo> Clone() override;
  bool BypassRedirectChecks() const override;

  // The |info| contains replacement factories for a subset of the existing
  // bundle.
  void Update(std::unique_ptr<URLLoaderFactoryBundleInfo> info);

 protected:
  ~URLLoaderFactoryBundle() override;

  // Returns a factory which can be used to acquire a loader for |request|.
  virtual network::mojom::URLLoaderFactory* GetFactory(
      const network::ResourceRequest& request);

  template <typename TKey>
  static std::map<TKey, network::mojom::URLLoaderFactoryPtrInfo>
  ClonePtrMapToPtrInfoMap(
      const std::map<TKey, network::mojom::URLLoaderFactoryPtr>& input) {
    std::map<TKey, network::mojom::URLLoaderFactoryPtrInfo> output;
    for (const auto& it : input) {
      const TKey& key = it.first;
      const network::mojom::URLLoaderFactoryPtr& factory = it.second;
      network::mojom::URLLoaderFactoryPtrInfo factory_info;
      factory->Clone(mojo::MakeRequest(&factory_info));
      output.emplace(key, std::move(factory_info));
    }
    return output;
  }

  network::mojom::URLLoaderFactoryPtr default_factory_;

  // Map from URL scheme to URLLoaderFactoryPtr for handling URL requests for
  // schemes not handled by the |default_factory_|.  See also
  // URLLoaderFactoryBundleInfo::SchemeMap and
  // ContentBrowserClient::SchemeToURLLoaderFactoryMap.
  using SchemeMap = std::map<std::string, network::mojom::URLLoaderFactoryPtr>;
  SchemeMap scheme_specific_factories_;

  // Map from origin of request initiator to URLLoaderFactoryPtr for handling
  // this initiator's requests. See also URLLoaderFactoryBundleInfo::OriginMap.
  using OriginMap = std::map<url::Origin, network::mojom::URLLoaderFactoryPtr>;
  OriginMap initiator_specific_factories_;

  bool bypass_redirect_checks_ = false;
};

}  // namespace content

#endif  // CONTENT_COMMON_URL_LOADER_FACTORY_BUNDLE_H_
