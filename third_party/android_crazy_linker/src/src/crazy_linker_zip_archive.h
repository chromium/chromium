// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRAZY_LINKER_ZIP_ARCHIVE_H
#define CRAZY_LINKER_ZIP_ARCHIVE_H

// Type definitions related to the ZIP archive format. Placed in their
// own header to make them available to unit-test sources.

#include <stdint.h>

namespace crazy {

// Template function used to verify that |TYPE| has a size of |EXPECTED_SIZE|
// bytes and also has an alignment of 1. Do not call directly, use
// CHECK_UNALIGNED_RECORD_TYPE below to perform compile-time checks.
template <typename TYPE, size_t EXPECTED_SIZE>
constexpr bool CheckUnalignedRecordType() {
  static_assert(sizeof(TYPE) == EXPECTED_SIZE, "invalid record definition");
  static_assert(alignof(TYPE) == 1, "invalid record alignment");
  return true;
}

// Check at compile type that |type_name| has |expected_size| bytes and
// and alignment of 1. This is required for all zip record definitions below.
#define CHECK_UNALIGNED_RECORD_TYPE(type_name, expected_size)         \
  static_assert(CheckUnalignedRecordType<type_name, expected_size>(), \
                "Incorrect " #type_name " definition")

// Values in the ZIP records are not necessarily aligned, so define
// zip_uint16_t and zip_uint32_t to have the same size as uint16_t and
// uint32_t respectively, but use an alignment of 1. Conversions operators are
// provided to read the values directly from memory.
struct zip_uint16_t {
  constexpr operator uint16_t() const {
    return static_cast<uint16_t>(data_[0]) |
           (static_cast<uint16_t>(data_[1]) << 8);
  }

  inline zip_uint16_t& operator=(uint16_t value) {
    data_[0] = static_cast<uint8_t>(value);
    data_[1] = static_cast<uint8_t>(value >> 8);
    return *this;
  }

  uint8_t data_[2] = {};
};
CHECK_UNALIGNED_RECORD_TYPE(zip_uint16_t, 2);

struct zip_uint32_t {
  constexpr operator uint32_t() const {
    return static_cast<uint32_t>(data_[0]) |
           (static_cast<uint32_t>(data_[1]) << 8) |
           (static_cast<uint32_t>(data_[2]) << 16) |
           (static_cast<uint32_t>(data_[3]) << 24);
  }

  inline zip_uint32_t& operator=(uint32_t value) {
    data_[0] = static_cast<uint8_t>(value);
    data_[1] = static_cast<uint8_t>(value >> 8);
    data_[2] = static_cast<uint8_t>(value >> 16);
    data_[3] = static_cast<uint8_t>(value >> 24);
    return *this;
  }

  uint8_t data_[4] = {};
};
CHECK_UNALIGNED_RECORD_TYPE(zip_uint32_t, 4);

// For an overview of the ZIP archive format, see
// http://www.pkware.com/documents/casestudies/APPNOTE.TXT
//

// 4.3.16  End of central directory record.
//
//       end of central dir signature    4 bytes  (0x06054b50)
//       number of this disk             2 bytes
//       number of the disk with the
//       start of the central directory  2 bytes
//       total number of entries in the
//       central directory on this disk  2 bytes
//       total number of entries in
//       the central directory           2 bytes
//       size of the central directory   4 bytes
//       offset of start of central
//       directory with respect to
//       the starting disk number        4 bytes
//       .ZIP file comment length        2 bytes
//       .ZIP file comment       (variable size)

struct ZipEndOfCentralDirectory {
  zip_uint32_t signature;
  zip_uint16_t ignored_disk_number;
  zip_uint16_t ignored_start_disk_number;
  zip_uint16_t ignored_num_disk_central_directory_entries;
  zip_uint16_t num_central_directory_entries;
  zip_uint32_t central_directory_length;
  zip_uint32_t central_directory_start;
  zip_uint16_t ignored_comment_length;

  static constexpr uint32_t kExpectedSignature = 0x06054b50;
};
CHECK_UNALIGNED_RECORD_TYPE(ZipEndOfCentralDirectory,
                            4 + 2 + 2 + 2 + 2 + 4 + 4 + 2);

// 4.3.12  Central directory structure:
//
//       [central directory header 1]
//       .
//       .
//       .
//       [central directory header n]
//       [digital signature]
//
//       File header:
//
//         central file header signature   4 bytes  (0x02014b50)
//         version made by                 2 bytes
//         version needed to extract       2 bytes
//         general purpose bit flag        2 bytes
//         compression method              2 bytes
//         last mod file time              2 bytes
//         last mod file date              2 bytes
//         crc-32                          4 bytes
//         compressed size                 4 bytes
//         uncompressed size               4 bytes
//         file name length                2 bytes
//         extra field length              2 bytes
//         file comment length             2 bytes
//         disk number start               2 bytes
//         internal file attributes        2 bytes
//         external file attributes        4 bytes
//         relative offset of local header 4 bytes
//
//         file name (variable size)
//         extra field (variable size)
//         file comment (variable size)

struct ZipCentralDirHeader {
  zip_uint32_t signature;
  zip_uint16_t ignored_version_made_by;
  zip_uint16_t ignored_version_extract;
  zip_uint16_t ignored_flags;
  zip_uint16_t ignored_compression_method;
  zip_uint16_t ignored_last_mod_file_time;
  zip_uint16_t ignored_last_mod_file_date;
  zip_uint32_t ignored_crc32;
  zip_uint32_t ignored_compressed_size;
  zip_uint32_t ignored_uncompressed_size;
  zip_uint16_t file_name_length;
  zip_uint16_t extra_field_length;
  zip_uint16_t file_comment_length;
  zip_uint16_t ignored_disk_number_start;
  zip_uint16_t ignored_internal_file_attributes;
  zip_uint32_t ignored_external_file_attributes;
  zip_uint32_t relative_offset_of_local_header;

  static constexpr uint32_t kExpectedSignature = 0x2014b50U;
};
CHECK_UNALIGNED_RECORD_TYPE(ZipCentralDirHeader,
                            4 + 2 + 2 + 2 + 2 + 2 + 2 + 4 + 4 + 4 + 2 + 2 + 2 +
                                2 + 2 + 4 + 4);

//    4.3.7  Local file header:
//
//       local file header signature     4 bytes  (0x04034b50)
//       version needed to extract       2 bytes
//       general purpose bit flag        2 bytes
//       compression method              2 bytes
//       last mod file time              2 bytes
//       last mod file date              2 bytes
//       crc-32                          4 bytes
//       compressed size                 4 bytes
//       uncompressed size               4 bytes
//       file name length                2 bytes
//       extra field length              2 bytes
//
//       file name (variable size)
//       extra field (variable size)

struct ZipLocalFileHeader {
  zip_uint32_t signature;
  zip_uint16_t ignored_version_extract;
  zip_uint16_t ignored_flags;
  zip_uint16_t compression_method;
  zip_uint16_t ignored_last_mod_file_time;
  zip_uint16_t ignored_last_mod_file_date;
  zip_uint32_t ignored_crc32;
  zip_uint32_t ignored_compressed_size;
  zip_uint32_t ignored_uncompressed_size;
  zip_uint16_t file_name_length;
  zip_uint16_t extra_field_length;

  static constexpr uint32_t kExpectedSignature = 0x04034b50;

  // Value for |compression_method| indicating the file is stored
  // without compression.
  static constexpr uint16_t kCompressionMethodStored = 0;
};

CHECK_UNALIGNED_RECORD_TYPE(ZipLocalFileHeader,
                            4 + 2 + 2 + 2 + 2 + 2 + 4 + 4 + 4 + 2 + 2);

}  // namespace crazy

#endif  // CRAZY_LINKER_ZIP_ARCHIVE_H
