// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_LOCAL_RESOURCE_URL_LOADER_FACTORY_H_
#define CONTENT_RENDERER_LOCAL_RESOURCE_URL_LOADER_FACTORY_H_

#include <cstdint>
#include <memory>

#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/socket/socket.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/blink/public/mojom/loader/local_resource_loader_config.mojom.h"
#include "url/origin.h"

namespace content {

// LocalResourceURLLoaderFactory is a URLLoaderFactory that lives in the
// renderer process and fetches resources directly from the ResourceBundle,
// enabling renderers to load bundled resources entirely in-process. This can
// significantly reduce IPC overhead for WebUIs, whose resources come almost
// exclusively from the ResourceBundle.
class CONTENT_EXPORT LocalResourceURLLoaderFactory
    : public network::mojom::URLLoaderFactory {
 public:
  struct Source {
    Source(blink::mojom::LocalResourceSourcePtr source,
           std::map<std::string, std::string> replacement_strings);
    Source(Source&& other);
    Source& operator=(Source&& other);
    ~Source();
    blink::mojom::LocalResourceSourcePtr source;
    std::map<std::string, std::string> replacement_strings;
  };

  LocalResourceURLLoaderFactory(
      blink::mojom::LocalResourceLoaderConfigPtr config,
      mojo::PendingRemote<network::mojom::URLLoaderFactory> fallback);
  ~LocalResourceURLLoaderFactory() override;

  // network::mojom::URLLoaderFactory implementation.
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> loader,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override;
  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver)
      override;

  // Fetches the resource from the ResourceBundle or browser-sent response map.
  // This might be called to load not only subresources, but also document body
  // loads.
  static scoped_refptr<base::RefCountedMemory> GetResource(
      const GURL& url,
      const blink::mojom::LocalResourceSourcePtr& source,
      const std::map<std::string, std::string>& replacement_strings,
      const std::string& mime_type);

 private:
  // CanServe returns true if and only if all of the following are true:
  //
  // 1. The LocalResourceURLLoaderFactory has a resource ID for the given URL
  //    (which depends on the resource metadata received from the browser
  //    process), and
  // 2. The resource ID corresponds to a resource that exists in
  //    'resources.pak'.
  //
  // It should return false in the following cases:
  //
  // 1. For dynamic resources that are generated on-the-fly in the browser
  //    process (e.g. strings.m.js, chrome://theme/colors.css).
  // 2. For static resources that reside in a pak file that is not
  //    memory-mapped in the renderer process (e.g. browser_tests.pak).
  //
  // CanServe is called internally for every resource request to decide whether
  // the request can be serviced in-process or if it has to be serviced
  // out-of-process.
  bool CanServe(const network::ResourceRequest& request) const;

  // Fetches the resource from the ResourceBundle or browser-sent response map,
  // and sends it to the URLLoaderClient. This is static because it is posted as
  // a task which may outlive |this|.
  static void GetResourceAndRespond(
      const scoped_refptr<base::RefCountedData<std::map<url::Origin, Source>>>
          sources,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client);

  // Map for resolving origins to their respective source metadata
  // (path-to-resource-ID mappings and string replacement maps).
  // It is ref-counted because it needs to be accessed in an async call to
  // GetResourceAndRespond, which may outlive |this|.
  const scoped_refptr<base::RefCountedData<std::map<url::Origin, Source>>>
      sources_;

  // Pipe to fallback factory, which should be the WebUIURLLoaderFactory in the
  // browser process. This is required because there are certain resources that
  // cannot be serviced in-process and must be serviced by the browser process.
  // See the CanServe method for more details.
  const mojo::Remote<network::mojom::URLLoaderFactory> fallback_;

  // Pipes to callers of the Clone method.
  mojo::ReceiverSet<network::mojom::URLLoaderFactory> receivers_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_LOCAL_RESOURCE_URL_LOADER_FACTORY_H_
