// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COURGETTE_ELF_TYPES_H_
#define COURGETTE_ELF_TYPES_H_

#include <stdint.h>

//
// This header defines various types from the ELF file spec, but no code
// related to using them.
//

typedef uint32_t Elf32_Addr;  // Unsigned program address
typedef uint16_t Elf32_Half;  // Unsigned medium integer
typedef uint32_t Elf32_Off;   // Unsigned file offset
typedef int32_t Elf32_Sword;  // Signed large integer
typedef uint32_t Elf32_Word;  // Unsigned large integer

// The header at the top of the file
struct Elf32_Ehdr {
  unsigned char  e_ident[16];
  Elf32_Half     e_type;
  Elf32_Half     e_machine;
  Elf32_Word     e_version;
  Elf32_Addr     e_entry;
  Elf32_Off      e_phoff;
  Elf32_Off      e_shoff;
  Elf32_Word     e_flags;
  Elf32_Half     e_ehsize;
  Elf32_Half     e_phentsize;
  Elf32_Half     e_phnum;
  Elf32_Half     e_shentsize;
  Elf32_Half     e_shnum;
  Elf32_Half     e_shstrndx;
};

// Indexes for header->e_ident[].
enum e_ident_indexes {
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
enum e_ident_class_values {
  ELFCLASSNONE = 0,  // Invalid class.
  ELFCLASS32 = 1,    // 32-bit objects.
  ELFCLASS64 = 2     // 64-bit objects.
};

// Values for header->e_ident[EI_DATA].
enum e_ident_data_values {
  ELFDATANONE = 0,  // Unknown data format.
  ELFDATA2LSB = 1,  // Two's complement, little-endian.
  ELFDATA2MSB = 2,  // Two's complement, big-endian.
};

// values for header->e_type
enum e_type_values {
  ET_NONE = 0,  // No file type
  ET_REL = 1,  // Relocatable file
  ET_EXEC = 2,  // Executable file
  ET_DYN = 3,  // Shared object file
  ET_CORE = 4,  // Core file
  ET_LOPROC = 0xff00,  // Processor-specific
  ET_HIPROC = 0xfff  // Processor-specific
};

// values for header->e_machine
enum e_machine_values {
  EM_NONE = 0,  // No machine
  EM_386 = 3,  // Intel Architecture
  EM_ARM = 40,  // ARM Architecture
  EM_x86_64 = 62,  // Intel x86-64 Architecture
  // Other values skipped
};

enum { SHN_UNDEF = 0 };

// A section header in the section header table
struct Elf32_Shdr {
  Elf32_Word   sh_name;
  Elf32_Word   sh_type;
  Elf32_Word   sh_flags;
  Elf32_Addr   sh_addr;
  Elf32_Off    sh_offset;
  Elf32_Word   sh_size;
  Elf32_Word   sh_link;
  Elf32_Word   sh_info;
  Elf32_Word   sh_addralign;
  Elf32_Word   sh_entsize;
};

// Values for the section type field in a section header
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
  SHT_HIPROC = 0x7fffffff,
  SHT_LOUSER = 0x80000000,
  SHT_HIUSER = 0xffffffff,
};

struct Elf32_Phdr {
  Elf32_Word    p_type;
  Elf32_Off     p_offset;
  Elf32_Addr    p_vaddr;
  Elf32_Addr    p_paddr;
  Elf32_Word    p_filesz;
  Elf32_Word    p_memsz;
  Elf32_Word    p_flags;
  Elf32_Word    p_align;
};

// Values for the segment type field in a program segment header
enum ph_type_values {
  PT_NULL = 0,
  PT_LOAD = 1,
  PT_DYNAMIC = 2,
  PT_INTERP = 3,
  PT_NOTE = 4,
  PT_SHLIB = 5,
  PT_PHDR = 6,
  PT_LOPROC = 0x70000000,
  PT_HIPROC = 0x7fffffff
};

struct Elf32_Rel {
  Elf32_Addr    r_offset;
  Elf32_Word    r_info;
};

struct Elf32_Rela {
  Elf32_Addr    r_offset;
  Elf32_Word    r_info;
  Elf32_Sword   r_addend;
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

enum elf32_rel_arm_type_values {
  R_ARM_RELATIVE = 23,
};

#endif  // COURGETTE_ELF_TYPES_H_
