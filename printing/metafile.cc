// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/metafile.h"

#include <stdint.h>

#include <vector>

#include "base/files/file.h"
#include "base/logging.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/numerics/safe_conversions.h"
#include "build/build_config.h"

namespace printing {

MetafilePlayer::MetafilePlayer() = default;

MetafilePlayer::~MetafilePlayer() = default;

Metafile::Metafile() = default;

Metafile::~Metafile() = default;

bool Metafile::GetDataAsVector(std::vector<char>* buffer) const {
  buffer->resize(GetDataSize());
  if (buffer->empty())
    return false;
  return GetData(&buffer->front(),
                 base::checked_cast<uint32_t>(buffer->size()));
}

base::MappedReadOnlyRegion Metafile::GetDataAsSharedMemoryRegion() const {
  uint32_t data_size = GetDataSize();
  if (data_size == 0) {
    DLOG(ERROR) << "Metafile has no data to map to a region.";
    return base::MappedReadOnlyRegion();
  }

  base::MappedReadOnlyRegion region_mapping =
      base::ReadOnlySharedMemoryRegion::Create(data_size);
  if (!region_mapping.IsValid()) {
    DLOG(ERROR) << "Failure mapping metafile data into region for size "
                << data_size;
    return base::MappedReadOnlyRegion();
  }

  if (!GetData(region_mapping.mapping.memory(), data_size)) {
    DLOG(ERROR) << "Failure getting metafile data into region";
    return base::MappedReadOnlyRegion();
  }

  return region_mapping;
}

#if !BUILDFLAG(IS_ANDROID)
bool Metafile::SaveTo(base::File* file) const {
  if (!file->IsValid())
    return false;

  std::vector<char> buffer;
  if (!GetDataAsVector(&buffer))
    return false;

  if (!file->WriteAtCurrentPosAndCheck(
          base::as_bytes(base::make_span(buffer)))) {
    DLOG(ERROR) << "Failed to save file.";
    return false;
  }
  return true;
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace printing
