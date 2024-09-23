// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_STORAGE_ACCESS_API_STATUS_H_
#define NET_STORAGE_ACCESS_API_STATUS_H_

namespace net {

// Status related to the Storage Access API.
// Spec: https://privacycg.github.io/storage-access/
enum class StorageAccessApiStatus {
  // This context has not opted into unpartitioned cookie access via the Storage
  // Access API.
  kNone,
  // This context has opted into unpartitioned cookie access via the Storage
  // Access API.
  kAccessViaAPI,
};

}  // namespace net

#endif  // NET_STORAGE_ACCESS_API_STATUS_H_
