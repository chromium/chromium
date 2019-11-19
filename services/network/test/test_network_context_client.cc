// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/test/test_network_context_client.h"

#include <utility>

#include "base/optional.h"
#include "base/task/post_task.h"
#include "base/threading/thread_restrictions.h"
#include "net/base/net_errors.h"

namespace network {

TestNetworkContextClient::TestNetworkContextClient() : receiver_(nullptr) {}

TestNetworkContextClient::TestNetworkContextClient(
    mojo::PendingReceiver<mojom::NetworkContextClient> receiver)
    : receiver_(this, std::move(receiver)) {}

TestNetworkContextClient::~TestNetworkContextClient() {}

void TestNetworkContextClient::OnFileUploadRequested(
    uint32_t process_id,
    bool async,
    const std::vector<base::FilePath>& file_paths,
    OnFileUploadRequestedCallback callback) {
  if (upload_files_invalid_) {
    std::move(callback).Run(net::ERR_ACCESS_DENIED, std::vector<base::File>());
    return;
  }
  base::ScopedAllowBlockingForTesting allow_blocking;
  uint32_t file_flags = base::File::FLAG_OPEN | base::File::FLAG_READ |
                        (async ? base::File::FLAG_ASYNC : 0);
  std::vector<base::File> files;
  for (base::FilePath path : file_paths) {
    files.emplace_back(path, file_flags);
    if (!files.back().IsValid()) {
      std::move(callback).Run(
          net::FileErrorToNetError(files.back().error_details()),
          std::vector<base::File>());
      return;
    }
  }

  if (ignore_last_upload_file_) {
    // Make the TestNetworkServiceClient respond one less file as requested.
    files.pop_back();
  }

  std::move(callback).Run(net::OK, std::move(files));
}

}  // namespace network
