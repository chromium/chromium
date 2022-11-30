// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_FILE_URL_LOADER_FACTORY_H_
#define CONTENT_BROWSER_LOADER_FILE_URL_LOADER_FACTORY_H_

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/cpp/self_deleting_url_loader_factory.h"

namespace content {

class SharedCorsOriginAccessList;

// A URLLoaderFactory used for the file:// scheme used when Network Service is
// enabled.
// If a caller needs a request that has a fetch request mode other than
// "no-cors", this class should be used on the UI thread.
class CONTENT_EXPORT FileURLLoaderFactory
    : public network::SelfDeletingURLLoaderFactory {
 public:
  // Returns mojo::PendingRemote to a newly constructed FileURLLoaderFactory.
  // The factory is self-owned - it will delete itself once there are no more
  // receivers (including the receiver associated with the returned
  // mojo::PendingRemote and the receivers bound by the Clone method).
  //
  // |shared_cors_origin_access_list| can be nullptr if only "no-cors" requests
  // will be made.
  //
  // Thread pool tasks posted by the constructed FileURLLoaderFactory use
  // |task_priority|.
  static mojo::PendingRemote<network::mojom::URLLoaderFactory> Create(
      const base::FilePath& profile_path,
      scoped_refptr<SharedCorsOriginAccessList> shared_cors_origin_access_list,
      base::TaskPriority task_priority);

  FileURLLoaderFactory(const FileURLLoaderFactory&) = delete;
  FileURLLoaderFactory& operator=(const FileURLLoaderFactory&) = delete;

 private:
  FileURLLoaderFactory(
      const base::FilePath& profile_path,
      scoped_refptr<SharedCorsOriginAccessList> shared_cors_origin_access_list,
      base::TaskPriority task_priority,
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> factory_receiver);

  // network::mojom::URLLoaderFactory:
  ~FileURLLoaderFactory() override;
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> loader,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override;

  void CreateLoaderAndStartInternal(
      const network::ResourceRequest request,
      network::mojom::FetchResponseType response_type,
      mojo::PendingReceiver<network::mojom::URLLoader> loader,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client);

  const base::FilePath profile_path_;
  const scoped_refptr<SharedCorsOriginAccessList>
      shared_cors_origin_access_list_;
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_FILE_URL_LOADER_FACTORY_H_
