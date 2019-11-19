// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_FILE_URL_LOADER_FACTORY_H_
#define CONTENT_BROWSER_LOADER_FILE_URL_LOADER_FACTORY_H_

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/threading/thread_checker.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace content {

class SharedCorsOriginAccessList;

// A URLLoaderFactory used for the file:// scheme used when Network Service is
// enabled.
// If a caller needs a request that has a fetch request mode other than
// "no-cors", this class should be used on the UI thread.
class CONTENT_EXPORT FileURLLoaderFactory
    : public network::mojom::URLLoaderFactory,
      public base::SupportsWeakPtr<FileURLLoaderFactory> {
 public:
  // |shared_cors_origin_access_list| can be nullptr if only "no-cors" requests
  // will be made. Thread pool tasks posted by the constructed
  // FileURLLoadedFactory use |priority|.
  FileURLLoaderFactory(
      const base::FilePath& profile_path,
      scoped_refptr<SharedCorsOriginAccessList> shared_cors_origin_access_list,
      base::TaskPriority task_priority);
  ~FileURLLoaderFactory() override;

 private:
  // network::mojom::URLLoaderFactory:
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> loader,
      int32_t routing_id,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override;
  void Clone(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> loader) override;

  void CreateLoaderAndStartInternal(
      const network::ResourceRequest request,
      mojo::PendingReceiver<network::mojom::URLLoader> loader,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      bool cors_flag);

  const base::FilePath profile_path_;
  const scoped_refptr<SharedCorsOriginAccessList>
      shared_cors_origin_access_list_;
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  mojo::ReceiverSet<network::mojom::URLLoaderFactory> receivers_;

  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(FileURLLoaderFactory);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_FILE_URL_LOADER_FACTORY_H_
