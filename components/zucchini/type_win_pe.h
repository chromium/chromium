// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ZUCCHINI_TYPE_WIN_PE_H_
#define COMPONENTS_ZUCCHINI_TYPE_WIN_PE_H_

#include <stddef.h>
#include <stdint.h>

namespace zucchini {

// Structures and constants taken from WINNT.h and following identical layout.
// This is used for parsing of Portable Executable (PE) file format.
namespace pe {
// Supported by MSVC, g++, and clang++. Ensures no gaps in packing.
#pragma pack(push, 1)

// IMAGE_NUMBEROF_DIRECTORY_ENTRIES
constexpr size_t kImageNumberOfDirectoryEntries = 16;

// IMAGE_FILE_BASE_RELOCATION_TABLE
constexpr size_t kIndexOfBaseRelocationTable = 5;

constexpr uint32_t kImageScnMemExecute = 0x20000000;  // IMAGE_SCN_MEM_EXECUTE
constexpr uint32_t kImageScnMemRead = 0x40000000;     // IMAGE_SCN_MEM_READ

// IMAGE_DOS_HEADER
struct ImageDOSHeader {
  uint16_t e_magic;  // 0x00
  uint16_t e_cblp;
  uint16_t e_cp;
  uint16_t e_crlc;
  uint16_t e_cparhdr;
  uint16_t e_minalloc;
  uint16_t e_maxalloc;
  uint16_t e_ss;
  uint16_t e_sp;  // 0x10
  uint16_t e_csum;
  uint16_t e_ip;
  uint16_t e_cs;
  uint16_t e_lfarlc;
  uint16_t e_ovno;
  uint16_t e_res[4];
  uint16_t e_oemid;  // 0x24
  uint16_t e_oeminfo;
  uint16_t e_res2[10];
  uint32_t e_lfanew;  // 0x3C
};
static_assert(sizeof(ImageDOSHeader) == 0x40,
              "DOS header size should be 0x40 bytes");

// IMAGE_SECTION_HEADER
struct ImageSectionHeader {
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
static_assert(sizeof(ImageSectionHeader) == 0x28,
              "Section header size should be 0x28 bytes");

// IMAGE_DATA_DIRECTORY
struct ImageDataDirectory {
  uint32_t virtual_address;
  uint32_t size;
};
static_assert(sizeof(ImageDataDirectory) == 0x08,
              "Data directory size should be 0x08 bytes");

// IMAGE_FILE_HEADER
struct ImageFileHeader {
  uint16_t machine;
  uint16_t number_of_sections;
  uint32_t time_date_stamp;
  uint32_t pointer_to_symbol_table;
  uint32_t number_of_symbols;
  uint16_t size_of_optional_header;
  uint16_t characteristics;
};
static_assert(sizeof(ImageFileHeader) == 0x14,
              "File header size should be 0x14 bytes");

// IMAGE_OPTIONAL_HEADER
struct ImageOptionalHeader {
  uint16_t magic;  // 0x00: 0x10B
  uint8_t major_linker_version;
  uint8_t minor_linker_version;
  uint32_t size_of_code;
  uint32_t size_of_initialized_data;
  uint32_t size_of_uninitialized_data;
  uint32_t address_of_entry_point;  // 0x10
  uint32_t base_of_code;
  uint32_t base_of_data;

  uint32_t image_base;
  uint32_t section_alignment;  // 0x20
  uint32_t file_alignment;
  uint16_t major_operating_system_version;
  uint16_t minor_operating_system_version;
  uint16_t major_image_version;
  uint16_t minor_image_version;
  uint16_t major_subsystem_version;  // 0x30
  uint16_t minor_subsystem_version;
  uint32_t win32_version_value;
  uint32_t size_of_image;
  uint32_t size_of_headers;
  uint32_t check_sum;  // 0x40
  uint16_t subsystem;
  uint16_t dll_characteristics;
  uint32_t size_of_stack_reserve;
  uint32_t size_of_stack_commit;
  uint32_t size_of_heap_reserve;  // 0x50
  uint32_t size_of_heap_commit;
  uint32_t loader_flags;
  uint32_t number_of_rva_and_sizes;

  // The number of elements is actually |number_of_rva_and_sizes|, so accesses
  // to |data_directory| should be checked against the bound.
  ImageDataDirectory data_directory[kImageNumberOfDirectoryEntries];  // 0x60
  /* 0xE0 */
};
static_assert(sizeof(ImageOptionalHeader) == 0xE0,
              "Optional header (32) size should be 0xE0 bytes");

// IMAGE_OPTIONAL_HEADER64
struct ImageOptionalHeader64 {
  uint16_t magic;  // 0x00: 0x20B
  uint8_t major_linker_version;
  uint8_t minor_linker_version;
  uint32_t size_of_code;
  uint32_t size_of_initialized_data;
  uint32_t size_of_uninitialized_data;
  uint32_t address_of_entry_point;  // 0x10
  uint32_t base_of_code;

  uint64_t image_base;
  uint32_t section_alignment;  // 0x20
  uint32_t file_alignment;
  uint16_t major_operating_system_version;
  uint16_t minor_operating_system_version;
  uint16_t major_image_version;
  uint16_t minor_image_version;
  uint16_t major_subsystem_version;  // 0x30
  uint16_t minor_subsystem_version;
  uint32_t win32_version_value;
  uint32_t size_of_image;
  uint32_t size_of_headers;
  uint32_t check_sum;  // 0x40
  uint16_t subsystem;
  uint16_t dll_characteristics;
  uint64_t size_of_stack_reserve;
  uint64_t size_of_stack_commit;  // 0x50
  uint64_t size_of_heap_reserve;
  uint64_t size_of_heap_commit;  // 0x60
  uint32_t loader_flags;
  uint32_t number_of_rva_and_sizes;
  ImageDataDirectory data_directory[kImageNumberOfDirectoryEntries];  // 0x70
  /* 0xF0 */
};
static_assert(sizeof(ImageOptionalHeader64) == 0xF0,
              "Optional header (64) size should be 0xF0 bytes");

struct RelocHeader {
  uint32_t rva_hi;
  uint32_t size;
};
static_assert(sizeof(RelocHeader) == 8, "RelocHeader size should be 8 bytes");

#pragma pack(pop)

}  // namespace pe

// Constants and offsets gleaned from WINNT.h and various articles on the
// format of Windows PE executables.

constexpr char const* kTextSectionName = ".text";

// Bitfield with characteristics usually associated with code sections.
const uint32_t kCodeCharacteristics =
    pe::kImageScnMemExecute | pe::kImageScnMemRead;

}  // namespace zucchini

#endif  // COMPONENTS_ZUCCHINI_TYPE_WIN_PE_H_
