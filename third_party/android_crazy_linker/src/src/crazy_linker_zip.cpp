// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crazy_linker_zip.h"

#include <assert.h>
#include <fcntl.h>
#include <inttypes.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "crazy_linker_debug.h"
#include "crazy_linker_system.h"
#include "crazy_linker_util.h"
#include "crazy_linker_zip_archive.h"

namespace crazy {
namespace {

// RAII pattern for unmapping and closing the mapped file.
class ScopedMMap {
 public:
  ScopedMMap(void* mem, uint32_t len) : mem_(mem), len_(len) {}
  ~ScopedMMap() {
    if (::munmap(mem_, len_) == -1) {
      LOG_ERRNO("munmap failed when trying to unmap zip file");
    }
  }
 private:
  void* mem_;
  uint32_t len_;
};

// Convenience class to safely read values from a specific memory range of
// bytes. Typical usage:
//
//   1) Create instance, providing an address in memory and a size in bytes.
//
//   2) Call CheckRange() method to verify that an (offset,size) tuple
//      describes a valid sub-range of the parent one.
//
//   3) Call AsRecordOf<TYPE>() to convert an offset into a pointer to
//      a TYPE instance within the parent range, or nullptr if the offset
//      is too large.
//
//   4) Use DataAt() to retrieve the address of data at a specific offset,
//      or nullptr if it is too large.
//
class MemoryRangeReader {
 public:
  // Constructor, creates a new instance that covers all memory in
  // |[memory, memory + size)|.
  constexpr MemoryRangeReader(const void* memory, size_t size)
      : base_(static_cast<const uint8_t*>(memory)), size_(size) {}

  // Returns true iff sub-range |[range_start, range_start + range_size)|
  // is fully contained within the bounds of this reader.
  constexpr bool CheckRange(size_t range_start, size_t range_size) const {
    return (range_start <= size_ && range_size <= size_ - range_start);
  }

  // Returns a pointer to the data at |[pos, pos + size)| if it is fully
  // contained within the range, or nullptr otherwise. NOTE: This always
  // return nullptr is |size| is 0.
  constexpr const uint8_t* DataAt(size_t pos, size_t size) const {
    return (size > 0 && CheckRange(pos, size)) ? base_ + pos : nullptr;
  }

  // Return a pointer to a record of type RECORD_TYPE from offset |pos|
  // in the current reader. This will return nullptr if |pos| is not valid
  // position for the record or if the range is too small to hold a record of
  // sizeof(RECORD_TYPE).
  template <typename RECORD_TYPE>
  constexpr const RECORD_TYPE* AsRecordOf(size_t pos) const {
    const size_t record_size = sizeof(RECORD_TYPE);
    return reinterpret_cast<const RECORD_TYPE*>(DataAt(pos, record_size));
  }

 private:
  const uint8_t* const base_ = nullptr;
  const size_t size_ = 0;
};

// Find the offset of |filename| within |zip_file|.
// |file_data| and |file_size| are the file's bytes and size.
// On success, return the offset. On failure return CRAZY_OFFSET_FAILED (-1)
int32_t FindFileOffsetInZipFile(const char* filename,
                                const char* zip_file,
                                const uint8_t* file_data,
                                size_t file_size) {
  // First, find the end of central directory record from the end of the file.
  if (file_size < sizeof(ZipEndOfCentralDirectory)) {
    LOG("The size of %s (%zu) is too small for a zip file", zip_file,
        file_size);
    return CRAZY_OFFSET_FAILED;
  }

  // NOTE: Safe due to check above.
  size_t end_offset = file_size - sizeof(ZipEndOfCentralDirectory);

  MemoryRangeReader file_reader(file_data, file_size);
  const ZipEndOfCentralDirectory* end_record;
  for (;;) {
    // Find end of central directory record from the end of the file.
    end_record = file_reader.AsRecordOf<ZipEndOfCentralDirectory>(end_offset);
    if (!end_record) {
      // NOTE: Should not happen, since |end_offset| is smaller than
      // |file_size - sizeof(ZipEndOfCentralDirectory)|. But better be safe
      // than sorry.
      return CRAZY_OFFSET_FAILED;
    }
    if (end_record->signature == end_record->kExpectedSignature)
      break;

    if (end_offset == 0) {
      LOG("Missing end of central directory in zip file %s", zip_file);
      return CRAZY_OFFSET_FAILED;
    }
    end_offset--;
  }

  const uint16_t num_entries = end_record->num_central_directory_entries;
  const uint32_t central_dir_length = end_record->central_directory_length;
  const uint32_t central_dir_start = end_record->central_directory_start;

  if (!file_reader.CheckRange(central_dir_start, central_dir_length)) {
    LOG("Found malformed central directory offset/length in %s", zip_file);
    return CRAZY_OFFSET_FAILED;
  }

  MemoryRangeReader central_dir_reader(file_data + central_dir_start,
                                       central_dir_length);

  // Parse all entries in the central directory until the entry for the
  // relevant file is found.
  const size_t expected_filename_len = strlen(filename);
  size_t local_header_offset = 0;
  size_t entry_offset = 0;
  for (size_t n = 0;;) {
    if (n >= num_entries) {
      LOG("Could not find entry for file '%s' in %s", filename, zip_file);
      return CRAZY_OFFSET_FAILED;
    }

    const auto* entry =
        central_dir_reader.AsRecordOf<ZipCentralDirHeader>(entry_offset);
    if (!entry || entry->signature != entry->kExpectedSignature) {
      LOG("Malformed central directory entry in %s", zip_file);
      return CRAZY_OFFSET_FAILED;
    }

    const uint16_t file_name_length = entry->file_name_length;
    const uint8_t* filename_bytes = central_dir_reader.DataAt(
        entry_offset + sizeof(*entry), file_name_length);
    if (!filename_bytes) {
      LOG("Malformed central directory file entry in zip file %s", zip_file);
      return CRAZY_OFFSET_FAILED;
    }

    if (file_name_length == expected_filename_len &&
        !::memcmp(filename_bytes, filename, expected_filename_len)) {
      // Found it!
      local_header_offset = entry->relative_offset_of_local_header;
      break;
    }

    // NOTE: Cannot overflow since all quantities are 16-bit values.
    const size_t entry_length = sizeof(*entry) + file_name_length +
                                entry->extra_field_length +
                                entry->file_comment_length;
    // Continue to next file.
    entry_offset += entry_length;
    n++;
  }

  // Now read the local header and compute the start offset.
  const auto* local_header =
      file_reader.AsRecordOf<ZipLocalFileHeader>(local_header_offset);
  if (!local_header ||
      local_header->signature != local_header->kExpectedSignature) {
    LOG("Invalid local file header offset %zu (max %zu) in zip file %s",
        local_header_offset, file_size - sizeof(*local_header), zip_file);
    return CRAZY_OFFSET_FAILED;
  }

  const uint16_t compression_method = local_header->compression_method;
  if (compression_method != local_header->kCompressionMethodStored) {
    LOG("%s is compressed within %s (found compression method %u, expected %u)",
        filename, zip_file, compression_method,
        local_header->kCompressionMethodStored);
    return CRAZY_OFFSET_FAILED;
  }

  const uint16_t file_name_length = local_header->file_name_length;
  const uint16_t extra_field_length = local_header->extra_field_length;

  // NOTE: Cannot overflow since all values are 16-bit.
  const uint32_t header_length =
      sizeof(*local_header) + file_name_length + extra_field_length;

  if (!file_reader.CheckRange(local_header_offset, header_length)) {
    LOG("Invalid local file header entry in zip file %s", zip_file);
    return CRAZY_OFFSET_FAILED;
  }

  // NOTE: Cannot overflow due to above check, since the file length
  // fits in a signed 32-bit integer, so does the offset.
  return static_cast<int32_t>(local_header_offset + header_length);
}

}  // unnamed namespace

int32_t FindStartOffsetOfFileInZipFile(const char* zip_file,
                                       const char* filename) {
  // Open the file
  FileDescriptor fd;
  if (!fd.OpenReadOnly(zip_file)) {
    LOG_ERRNO("open failed trying to open zip file %s", zip_file);
    return CRAZY_OFFSET_FAILED;
  }

  // Find the length of the file.
  int64_t file_size64 = fd.GetFileSize();
  if (file_size64 < 0) {
    LOG_ERRNO("stat failed trying to stat zip file %s", zip_file);
    return CRAZY_OFFSET_FAILED;
  }
  if (file_size64 > (int64_t(1) << 31)) {
    LOG("Zip archive too large (%" PRId64 " bytes): %s", file_size64, zip_file);
    return CRAZY_OFFSET_FAILED;
  }
  // NOTE: This cannot fail due to the check above.
  size_t file_size = static_cast<size_t>(file_size64);
  if (file_size == 0) {
    LOG("Empty zip archive: %s", zip_file);
    return CRAZY_OFFSET_FAILED;
  }

  // Map the file into memory.
  void* mem = fd.Map(NULL, file_size, PROT_READ, MAP_PRIVATE, 0);
  if (mem == MAP_FAILED || mem == NULL) {
    LOG_ERRNO("mmap failed trying to mmap zip file %s", zip_file);
    return CRAZY_OFFSET_FAILED;
  }
  ScopedMMap scoped_mmap(mem, file_size);

  return FindFileOffsetInZipFile(filename, zip_file, static_cast<uint8_t*>(mem),
                                 file_size);
}

}  // crazy namespace

// Define this macro when compiling this source file for fuzzing.
#if defined(CRAZY_LINKER_ENABLE_FUZZING)
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  crazy::FindFileOffsetInZipFile("dummy-file.txt", "dummy.zip", data, size);
  return 0;
}
#endif  // CRAZY_LINKER_ENABLE_FUZZING
