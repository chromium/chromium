// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/font_unique_name_lookup/font_table_persistence.h"

#include <optional>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/hash/hash.h"
#include "base/pickle.h"
#include "base/threading/scoped_blocking_call.h"

namespace blink {

namespace font_table_persistence {

bool LoadFromFile(base::FilePath file_path,
                  base::MappedReadOnlyRegion* name_table_region) {
  DCHECK(!file_path.empty());
  // Reset to empty to ensure IsValid() is false if reading fails.
  *name_table_region = base::MappedReadOnlyRegion();
  std::vector<char> file_contents;
  {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);

    base::File table_cache_file(
        file_path, base::File::FLAG_OPEN | base::File::Flags::FLAG_READ);
    if (!table_cache_file.IsValid() || table_cache_file.GetLength() <= 0) {
      return false;
    }

    file_contents.resize(table_cache_file.GetLength());

    if (UNSAFE_TODO(table_cache_file.Read(0, file_contents.data(),
                                          file_contents.size())) <= 0) {
      return false;
    }
  }

  base::Pickle pickle =
      base::Pickle::WithUnownedBuffer(base::as_byte_span(file_contents));
  base::PickleIterator pickle_iterator(pickle);

  uint32_t checksum = 0;
  if (!pickle_iterator.ReadUInt32(&checksum)) {
    return false;
  }

  std::optional<base::span<const uint8_t>> read_result =
      pickle_iterator.ReadData();
  if (!read_result.has_value()) {
    return false;
  }
  base::span<const uint8_t> proto = read_result.value();
  if (proto.empty()) {
    return false;
  }

  if (checksum != base::PersistentHash(proto)) {
    return false;
  }

  blink::FontUniqueNameTable font_table;
  if (!font_table.ParseFromArray(proto.data(), proto.size())) {
    return false;
  }

  *name_table_region = base::ReadOnlySharedMemoryRegion::Create(proto.size());
  if (!name_table_region->IsValid() || !name_table_region->mapping.size()) {
    return false;
  }

  base::span(name_table_region->mapping).copy_from(proto);

  return true;
}

bool PersistToFile(const base::MappedReadOnlyRegion& name_table_region,
                   base::FilePath file_path) {
  DCHECK(name_table_region.mapping.IsValid());
  DCHECK(name_table_region.mapping.size());
  DCHECK(!file_path.empty());

  base::File table_cache_file(file_path, base::File::FLAG_CREATE_ALWAYS |
                                             base::File::Flags::FLAG_WRITE);
  if (!table_cache_file.IsValid()) {
    return false;
  }

  base::Pickle pickle;
  uint32_t checksum = base::PersistentHash(name_table_region.mapping);
  pickle.WriteUInt32(checksum);
  pickle.WriteData(name_table_region.mapping);
  DCHECK(pickle.size());
  {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);

    if (UNSAFE_TODO(table_cache_file.Write(0, pickle.data_as_char(),
                                           pickle.size())) == -1) {
      table_cache_file.SetLength(0);
      return false;
    }
  }
  return true;
}

}  // namespace font_table_persistence

}  // namespace blink
