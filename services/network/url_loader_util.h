// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_URL_LOADER_UTIL_H_
#define SERVICES_NETWORK_URL_LOADER_UTIL_H_

#include <memory>
#include <vector>

#include "base/files/file.h"

namespace base {
class SequencedTaskRunner;
}  // namespace  base

namespace net {
class UploadDataStream;
}  // namespace  net

namespace network {

class ResourceRequestBody;

// Creates a net::UploadDataStream from the passed `body` and `opened_files`.
// `file_task_runner` will be used for reading file elements in the `body`.
std::unique_ptr<net::UploadDataStream> CreateUploadDataStream(
    ResourceRequestBody* body,
    std::vector<base::File>& opened_files,
    base::SequencedTaskRunner* file_task_runner);

}  // namespace network

#endif  // SERVICES_NETWORK_URL_LOADER_UTIL_H_
