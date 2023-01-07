// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/file_enumeration_entry_mojom_traits.h"

#include "base/files/file_path.h"
#include "base/time/time.h"

namespace mojo {

bool StructTraits<network::mojom::FileEnumerationEntryDataView,
                  disk_cache::BackendFileOperations::FileEnumerationEntry>::
    Read(network::mojom::FileEnumerationEntryDataView view,
         disk_cache::BackendFileOperations::FileEnumerationEntry* out) {
  base::FilePath path;
  base::Time last_accessed;
  base::Time last_modified;
  if (!view.ReadPath(&path) || !view.ReadLastAccessed(&last_accessed) ||
      !view.ReadLastModified(&last_modified)) {
    return false;
  }
  *out = disk_cache::BackendFileOperations::FileEnumerationEntry(
      std::move(path), view.size(), last_accessed, last_modified);
  return true;
}

}  // namespace mojo
