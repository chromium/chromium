// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/font_table_linux.h"

#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <limits>
#include <memory>

#include "base/numerics/safe_conversions.h"
#include "base/posix/eintr_wrapper.h"
#include "base/sys_byteorder.h"

namespace pdf {

// TODO(drott): This should be should be replaced with using FreeType for the
// purpose instead of reimplementing table parsing.
bool GetFontTable(int fd,
                  uint32_t table_tag,
                  off_t offset,
                  uint8_t* output,
                  size_t* output_length) {
  if (offset < 0)
    return false;

  size_t data_length = 0;  // the length of the file data.
  off_t data_offset = 0;   // the offset of the data in the file.
  if (table_tag == 0) {
    // Get the entire font file.
    struct stat st;
    if (fstat(fd, &st) < 0)
      return false;
    data_length = base::checked_cast<size_t>(st.st_size);
  } else {
    // Get a font table. Read the header to find its offset in the file.
    uint16_t num_tables;
    ssize_t n = HANDLE_EINTR(
        pread(fd, &num_tables, sizeof(num_tables), 4 /* skip the font type */));
    if (n != sizeof(num_tables))
      return false;
    // Font data is stored in net (big-endian) order.
    num_tables = base::NetToHost16(num_tables);

    // Read the table directory.
    static const size_t kTableEntrySize = 16;
    const size_t directory_size = num_tables * kTableEntrySize;
    std::unique_ptr<uint8_t[]> table_entries(new uint8_t[directory_size]);
    n = HANDLE_EINTR(pread(fd, table_entries.get(), directory_size,
                           12 /* skip the SFNT header */));
    if (n != base::checked_cast<ssize_t>(directory_size))
      return false;

    for (uint16_t i = 0; i < num_tables; ++i) {
      uint8_t* entry = table_entries.get() + i * kTableEntrySize;
      uint32_t tag = *reinterpret_cast<uint32_t*>(entry);
      if (tag == table_tag) {
        // Font data is stored in net (big-endian) order.
        data_offset =
            base::NetToHost32(*reinterpret_cast<uint32_t*>(entry + 8));
        data_length =
            base::NetToHost32(*reinterpret_cast<uint32_t*>(entry + 12));
        break;
      }
    }
  }

  if (!data_length)
    return false;
  // Clamp |offset| inside the allowable range. This allows the read to succeed
  // but return 0 bytes.
  offset = std::min(offset, base::checked_cast<off_t>(data_length));
  // Make sure it's safe to add the data offset and the caller's logical offset.
  // Define the maximum positive offset on 32 bit systems.
  static const off_t kMaxPositiveOffset32 = 0x7FFFFFFF;  // 2 GB - 1.
  if ((offset > kMaxPositiveOffset32 / 2) ||
      (data_offset > kMaxPositiveOffset32 / 2))
    return false;
  data_offset += offset;
  data_length -= offset;

  if (output) {
    // 'output_length' holds the maximum amount of data the caller can accept.
    data_length = std::min(data_length, *output_length);
    ssize_t n = HANDLE_EINTR(pread(fd, output, data_length, data_offset));
    if (n != base::checked_cast<ssize_t>(data_length))
      return false;
  }
  *output_length = data_length;

  return true;
}

}  // namespace pdf
