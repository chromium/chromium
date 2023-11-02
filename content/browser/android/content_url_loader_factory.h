// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_CONTENT_URL_LOADER_FACTORY_H_
#define CONTENT_BROWSER_ANDROID_CONTENT_URL_LOADER_FACTORY_H_

#include "base/memory/ref_counted.h"
#include "base/task/sequenced_task_runner.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/cpp/self_deleting_url_loader_factory.h"

namespace content {

// A URLLoaderFactory used for the content:// scheme used when Network Service
// is enabled.
class CONTENT_EXPORT ContentURLLoaderFactory
    : public network::SelfDeletingURLLoaderFactory {
 public:
  // Returns mojo::PendingRemote to a newly constructed ContentURLLoadedFactory.
  // The factory is self-owned - it will delete itself once there are no more
  // receivers (including the receiver associated with the returned
  // mojo::PendingRemote and the receivers bound by the Clone method).
  static mojo::PendingRemote<network::mojom::URLLoaderFactory> Create();

  ContentURLLoaderFactory(const ContentURLLoaderFactory&) = delete;
  ContentURLLoaderFactory& operator=(const ContentURLLoaderFactory&) = delete;

 private:
  // SequencedTaskRunner must be allowed to block and should have background
  // priority since it will be used to schedule synchronous file I/O tasks.
  ContentURLLoaderFactory(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> factory_receiver);

  // network::mojom::URLLoaderFactory:
  ~ContentURLLoaderFactory() override;
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> loader,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override;

  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_CONTENT_URL_LOADER_FACTORY_H_
