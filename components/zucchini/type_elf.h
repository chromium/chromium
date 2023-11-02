// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ZUCCHINI_TYPE_ELF_H_
#define COMPONENTS_ZUCCHINI_TYPE_ELF_H_

#include <stdint.h>

namespace zucchini {

// Structures and constants taken from linux/elf.h and following identical
// layout. This is used for parsing of Executable and Linkable Format (ELF).
namespace elf {
// Supported by MSVC, g++, and clang++. Ensures no gaps in packing.
#pragma pack(push, 1)

// This header defines various types from the ELF file spec, but no code
// related to using them.

typedef uint32_t Elf32_Addr;  // Unsigned program address.
typedef uint16_t Elf32_Half;  // Unsigned medium integer.
typedef uint32_t Elf32_Off;   // Unsigned file offset.
typedef int32_t Elf32_Sword;  // Signed large integer.
typedef uint32_t Elf32_Word;  // Unsigned large integer.

typedef uint64_t Elf64_Addr;   // Unsigned program address.
typedef uint16_t Elf64_Half;   // Unsigned medium integer.
typedef uint64_t Elf64_Off;    // Unsigned file offset.
typedef int32_t Elf64_Sword;   // Signed large integer.
typedef uint32_t Elf64_Word;   // Unsigned large integer.
typedef int64_t Elf64_Sxword;  // Signed extra large integer.
typedef uint64_t Elf64_Xword;  // Unsigned extra large integer.

// The header at the top of the file.
struct Elf32_Ehdr {
  unsigned char e_ident[16];
  Elf32_Half e_type;
  Elf32_Half e_machine;
  Elf32_Word e_version;
  Elf32_Addr e_entry;
  Elf32_Off e_phoff;
  Elf32_Off e_shoff;
  Elf32_Word e_flags;
  Elf32_Half e_ehsize;
  Elf32_Half e_phentsize;
  Elf32_Half e_phnum;
  Elf32_Half e_shentsize;
  Elf32_Half e_shnum;
  Elf32_Half e_shstrndx;
};

struct Elf64_Ehdr {
  unsigned char e_ident[16];
  Elf64_Half e_type;
  Elf64_Half e_machine;
  Elf64_Word e_version;
  Elf64_Addr e_entry;
  Elf64_Off e_phoff;
  Elf64_Off e_shoff;
  Elf64_Word e_flags;
  Elf64_Half e_ehsize;
  Elf64_Half e_phentsize;
  Elf64_Half e_phnum;
  Elf64_Half e_shentsize;
  Elf64_Half e_shnum;
  Elf64_Half e_shstrndx;
};

// Identification Indexes in header->e_ident.
enum IdentificationIndex {
  EI_MAG0 = 0,        // File identification.
  EI_MAG1 = 1,        // File identification.
  EI_MAG2 = 2,        // File identification.
  EI_MAG3 = 3,        // File identification.
  EI_CLASS = 4,       // File class.
  EI_DATA = 5,        // Data encoding.
  EI_VERSION = 6,     // File version.
  EI_OSABI = 7,       // Operating system/ABI identification.
  EI_ABIVERSION = 8,  // ABI version.
  EI_PAD = 9,         // Start of padding bytes.
  EI_NIDENT = 16      // Size of e_ident[].
};

// Values for header->e_ident[EI_CLASS].
enum FileClass {
  ELFCLASSNONE = 0,  // Invalid class.
  ELFCLASS32 = 1,    // 32-bit objects.
  ELFCLASS64 = 2     // 64-bit objects.
};

// Values for header->e_type.
enum FileType {
  ET_NONE = 0,         // No file type
  ET_REL = 1,          // Relocatable file
  ET_EXEC = 2,         // Executable file
  ET_DYN = 3,          // Shared object file
  ET_CORE = 4,         // Core file
  ET_LOPROC = 0xFF00,  // Processor-specific
  ET_HIPROC = 0xFFFF   // Processor-specific
};

// Values for header->e_machine.
enum MachineArchitecture {
  EM_NONE = 0,       // No machine.
  EM_386 = 3,        // Intel Architecture.
  EM_ARM = 40,       // ARM Architecture.
  EM_X86_64 = 62,    // Intel x86-64 Architecture.
  EM_AARCH64 = 183,  // ARM Architecture, 64-bit.
  // Other values skipped.
};

// A section header in the section header table.
struct Elf32_Shdr {
  Elf32_Word sh_name;
  Elf32_Word sh_type;
  Elf32_Word sh_flags;
  Elf32_Addr sh_addr;
  Elf32_Off sh_offset;
  Elf32_Word sh_size;
  Elf32_Word sh_link;
  Elf32_Word sh_info;
  Elf32_Word sh_addralign;
  Elf32_Word sh_entsize;
};

struct Elf64_Shdr {
  Elf64_Word sh_name;
  Elf64_Word sh_type;
  Elf64_Xword sh_flags;
  Elf64_Addr sh_addr;
  Elf64_Off sh_offset;
  Elf64_Xword sh_size;
  Elf64_Word sh_link;
  Elf64_Word sh_info;
  Elf64_Xword sh_addralign;
  Elf64_Xword sh_entsize;
};

// Values for the section type field in a section header.
enum sh_type_values {
  SHT_NULL = 0,
  SHT_PROGBITS = 1,
  SHT_SYMTAB = 2,
  SHT_STRTAB = 3,
  SHT_RELA = 4,
  SHT_HASH = 5,
  SHT_DYNAMIC = 6,
  SHT_NOTE = 7,
  SHT_NOBITS = 8,
  SHT_REL = 9,
  SHT_SHLIB = 10,
  SHT_DYNSYM = 11,
  SHT_INIT_ARRAY = 14,
  SHT_FINI_ARRAY = 15,
  SHT_LOPROC = 0x70000000,
  SHT_HIPROC = 0x7FFFFFFF,
  SHT_LOUSER = 0x80000000,
  SHT_HIUSER = 0xFFFFFFFF
};

enum sh_flag_masks {
  SHF_WRITE = 1 << 0,
  SHF_ALLOC = 1 << 1,
  SHF_EXECINSTR = 1 << 2,
  // 1 << 3 is reserved.
  SHF_MERGE = 1 << 4,
  SHF_STRINGS = 1 << 5,
  SHF_INFO_LINK = 1 << 6,
  SHF_LINK_ORDER = 1 << 7,
  SHF_OS_NONCONFORMING = 1 << 8,
  SHF_GROUP = 1 << 9,
  SHF_TLS = 1 << 10,
  SHF_COMPRESSED = 1 << 11,
};

struct Elf32_Phdr {
  Elf32_Word p_type;
  Elf32_Off p_offset;
  Elf32_Addr p_vaddr;
  Elf32_Addr p_paddr;
  Elf32_Word p_filesz;
  Elf32_Word p_memsz;
  Elf32_Word p_flags;
  Elf32_Word p_align;
};

struct Elf64_Phdr {
  Elf64_Word p_type;
  Elf64_Word p_flags;
  Elf64_Off p_offset;
  Elf64_Addr p_vaddr;
  Elf64_Addr p_paddr;
  Elf64_Xword p_filesz;
  Elf64_Xword p_memsz;
  Elf64_Xword p_align;
};

// Values for the segment type field in a program segment header.
enum ph_type_values {
  PT_NULL = 0,
  PT_LOAD = 1,
  PT_DYNAMIC = 2,
  PT_INTERP = 3,
  PT_NOTE = 4,
  PT_SHLIB = 5,
  PT_PHDR = 6,
  PT_LOPROC = 0x70000000,
  PT_HIPROC = 0x7FFFFFFF
};

struct Elf32_Rel {
  Elf32_Addr r_offset;
  Elf32_Word r_info;
};

struct Elf64_Rel {
  Elf64_Addr r_offset;
  Elf64_Xword r_info;
};

struct Elf32_Rela {
  Elf32_Addr r_offset;
  Elf32_Word r_info;
  Elf32_Sword r_addend;
};

struct Elf64_Rela {
  Elf64_Addr r_offset;
  Elf64_Xword r_info;
  Elf64_Sxword r_addend;
};

enum elf32_rel_386_type_values {
  R_386_NONE = 0,
  R_386_32 = 1,
  R_386_PC32 = 2,
  R_386_GOT32 = 3,
  R_386_PLT32 = 4,
  R_386_COPY = 5,
  R_386_GLOB_DAT = 6,
  R_386_JMP_SLOT = 7,
  R_386_RELATIVE = 8,
  R_386_GOTOFF = 9,
  R_386_GOTPC = 10,
  R_386_TLS_TPOFF = 14,
};

enum elf32_rel_x86_64_type_values {
  R_X86_64_NONE = 0,
  R_X86_64_64 = 1,
  R_X86_64_PC32 = 2,
  R_X86_64_GOT32 = 3,
  R_X86_64_PLT32 = 4,
  R_X86_64_COPY = 5,
  R_X86_64_GLOB_DAT = 6,
  R_X86_64_JUMP_SLOT = 7,
  R_X86_64_RELATIVE = 8,
  R_X86_64_GOTPCREL = 9,
  R_X86_64_32 = 10,
  R_X86_64_32S = 11,
  R_X86_64_16 = 12,
  R_X86_64_PC16 = 13,
  R_X86_64_8 = 14,
  R_X86_64_PC8 = 15,
};

enum elf32_rel_arm_type_values {
  R_ARM_RELATIVE = 23,
};

enum elf64_rel_aarch64_type_values {
  R_AARCH64_GLOB_DAT = 0x401,
  R_AARCH64_JUMP_SLOT = 0x402,
  R_AARCH64_RELATIVE = 0x403,
};

#pragma pack(pop)

}  // namespace elf
}  // namespace zucchini

#endif  // COMPONENTS_ZUCCHINI_TYPE_ELF_H_
