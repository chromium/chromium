// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_FILE_ENUMERATION_ENTRY_MOJOM_TRAITS_H_
#define SERVICES_NETWORK_PUBLIC_CPP_FILE_ENUMERATION_ENTRY_MOJOM_TRAITS_H_

#include "mojo/public/cpp/bindings/struct_traits.h"
#include "net/disk_cache/disk_cache.h"
#include "services/network/public/mojom/http_cache_backend_file_operations.mojom.h"

namespace mojo {

template <>
struct StructTraits<network::mojom::FileEnumerationEntryDataView,
                    disk_cache::BackendFileOperations::FileEnumerationEntry> {
  static const base::FilePath& path(
      const disk_cache::BackendFileOperations::FileEnumerationEntry& entry) {
    return entry.path;
  }
  static int64_t size(
      const disk_cache::BackendFileOperations::FileEnumerationEntry& entry) {
    return entry.size;
  }
  static base::Time last_accessed(
      const disk_cache::BackendFileOperations::FileEnumerationEntry& entry) {
    return entry.last_accessed;
  }
  static base::Time last_modified(
      const disk_cache::BackendFileOperations::FileEnumerationEntry& entry) {
    return entry.last_modified;
  }

  static bool Read(
      network::mojom::FileEnumerationEntryDataView view,
      disk_cache::BackendFileOperations::FileEnumerationEntry* out);
};

}  // namespace mojo

#endif  // SERVICES_NETWORK_PUBLIC_CPP_FILE_ENUMERATION_ENTRY_MOJOM_TRAITS_H_
