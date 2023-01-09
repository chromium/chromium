// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_WEBENGINE_BROWSER_CONTENT_DIRECTORY_LOADER_FACTORY_H_
#define FUCHSIA_WEB_WEBENGINE_BROWSER_CONTENT_DIRECTORY_LOADER_FACTORY_H_

#include <fuchsia/io/cpp/fidl.h>
#include <fuchsia/web/cpp/fidl.h>
#include <lib/fidl/cpp/interface_handle.h>

#include "base/task/sequenced_task_runner.h"
#include "fuchsia_web/webengine/web_engine_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/network/public/cpp/self_deleting_url_loader_factory.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

// URLLoaderFactory which services requests for resources stored under named
// directories. The directories are accessed using the fuchsia-dir:// scheme.
class ContentDirectoryLoaderFactory
    : public network::SelfDeletingURLLoaderFactory {
 public:
  // Path in the browser process' namespace at which content directories
  // should be mounted.
  static WEB_ENGINE_EXPORT const char kContentDirectoriesPath[];

  // Returns mojo::PendingRemote to a newly constructed
  // ContentDirectoryLoaderFactory.  The factory is self-owned - it will delete
  // itself once there are no more receivers (including the receiver associated
  // with the returned mojo::PendingRemote and the receivers bound by the Clone
  // method).
  static mojo::PendingRemote<network::mojom::URLLoaderFactory> Create();

  ContentDirectoryLoaderFactory(const ContentDirectoryLoaderFactory&) = delete;
  ContentDirectoryLoaderFactory& operator=(
      const ContentDirectoryLoaderFactory&) = delete;

 private:
  explicit ContentDirectoryLoaderFactory(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> factory_receiver);
  ~ContentDirectoryLoaderFactory() override;

  // network::mojom::URLLoaderFactory:
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> loader,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) final;

  // Used for executing blocking URLLoader routines.
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

#endif  // FUCHSIA_WEB_WEBENGINE_BROWSER_CONTENT_DIRECTORY_LOADER_FACTORY_H_
