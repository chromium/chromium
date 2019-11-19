// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COURGETTE_TYPES_WIN_PE_H_
#define COURGETTE_TYPES_WIN_PE_H_

#include <stddef.h>
#include <stdint.h>

namespace courgette {

// PE file section header.  This struct has the same layout as the
// IMAGE_SECTION_HEADER structure from WINNT.H
// http://msdn.microsoft.com/en-us/library/ms680341(VS.85).aspx
//
#pragma pack(push, 1)  // Supported by MSVC and GCC. Ensures no gaps in packing.
struct Section {
  char name[8];
  uint32_t virtual_size;
  uint32_t virtual_address;
  uint32_t size_of_raw_data;
  uint32_t file_offset_of_raw_data;
  uint32_t pointer_to_relocations;   // Always zero in an image.
  uint32_t pointer_to_line_numbers;  // Always zero in an image.
  uint16_t number_of_relocations;    // Always zero in an image.
  uint16_t number_of_line_numbers;   // Always zero in an image.
  uint32_t characteristics;
};
#pragma pack(pop)

static_assert(sizeof(Section) == 40, "section size is 40 bytes");

// ImageDataDirectory has same layout as IMAGE_DATA_DIRECTORY structure from
// WINNT.H
// http://msdn.microsoft.com/en-us/library/ms680305(VS.85).aspx
//
class ImageDataDirectory {
 public:
  ImageDataDirectory() : address_(0), size_(0) {}
  RVA address_;
  uint32_t size_;
};

static_assert(sizeof(ImageDataDirectory) == 8,
              "image data directory size is 8 bytes");


////////////////////////////////////////////////////////////////////////////////

// Constants and offsets gleaned from WINNT.H and various articles on the
// format of Windows PE executables.

// This is FIELD_OFFSET(IMAGE_DOS_HEADER, e_lfanew):
const size_t kOffsetOfFileAddressOfNewExeHeader = 0x3c;

const uint16_t kImageNtOptionalHdr32Magic = 0x10b;
const uint16_t kImageNtOptionalHdr64Magic = 0x20b;

const size_t kSizeOfCoffHeader = 20;
const size_t kMinPeHeaderSize = 4 /*signature*/ + kSizeOfCoffHeader;
const size_t kOffsetOfDataDirectoryFromImageOptionalHeader32 = 96;
const size_t kOffsetOfDataDirectoryFromImageOptionalHeader64 = 112;

}  // namespace courgette

#endif  // COURGETTE_TYPES_WIN_PE_H_
