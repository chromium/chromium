// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_CONTENT_URL_LOADER_FACTORY_H_
#define CONTENT_BROWSER_ANDROID_CONTENT_URL_LOADER_FACTORY_H_

#include "base/macros.h"
#include "base/sequenced_task_runner.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace content {

// A URLLoaderFactory used for the content:// scheme used when Network Service
// is enabled.
class CONTENT_EXPORT ContentURLLoaderFactory
    : public network::mojom::URLLoaderFactory {
 public:
  // SequencedTaskRunner must be allowed to block and should have background
  // priority since it will be used to schedule synchronous file I/O tasks.
  explicit ContentURLLoaderFactory(
      scoped_refptr<base::SequencedTaskRunner> task_runner);
  ~ContentURLLoaderFactory() override;

 private:
  // network::mojom::URLLoaderFactory:
  void CreateLoaderAndStart(network::mojom::URLLoaderRequest loader,
                            int32_t routing_id,
                            int32_t request_id,
                            uint32_t options,
                            const network::ResourceRequest& request,
                            network::mojom::URLLoaderClientPtr client,
                            const net::MutableNetworkTrafficAnnotationTag&
                                traffic_annotation) override;
  void Clone(network::mojom::URLLoaderFactoryRequest loader) override;

  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  mojo::BindingSet<network::mojom::URLLoaderFactory> bindings_;

  DISALLOW_COPY_AND_ASSIGN(ContentURLLoaderFactory);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_CONTENT_URL_LOADER_FACTORY_H_
