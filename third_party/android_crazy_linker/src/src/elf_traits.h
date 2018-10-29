// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _ELF_TRAITS_H_
#define _ELF_TRAITS_H_

// NOTE: <stdint.h> is required here before <elf.h>. This is a NDK header bug.
#include <stdint.h>
#include <elf.h>

// ELF is a traits structure used to provide convenient aliases for
// 32/64 bit Elf types, depending on the target CPU bitness.
#if __SIZEOF_POINTER__ == 4
struct ELF {
  typedef Elf32_Ehdr Ehdr;
  typedef Elf32_Phdr Phdr;
  typedef Elf32_Word Word;
  typedef Elf32_Sword Sword;
  // TODO(simonb): Elf32_Sxword missing from the NDK.
  typedef int64_t Sxword;
  typedef Elf32_Addr Addr;
  typedef Elf32_Dyn Dyn;
  typedef Elf32_Sym Sym;
  typedef Elf32_Rel Rel;
  typedef Elf32_Rela Rela;
  typedef Elf32_Word Relr;
  typedef Elf32_auxv_t auxv_t;

  enum { kElfClass = ELFCLASS32 };
  enum { kElfBits = 32 };

# ifndef ELF_R_TYPE
# define ELF_R_TYPE ELF32_R_TYPE
# endif

# ifndef ELF_R_SYM
# define ELF_R_SYM ELF32_R_SYM
# endif

# ifndef ELF_R_INFO
# define ELF_R_INFO ELF32_R_INFO
# endif
};
#elif __SIZEOF_POINTER__ == 8
struct ELF {
  typedef Elf64_Ehdr Ehdr;
  typedef Elf64_Phdr Phdr;
  typedef Elf64_Word Word;
  typedef Elf64_Sword Sword;
  typedef Elf64_Sxword Sxword;
  typedef Elf64_Addr Addr;
  typedef Elf64_Dyn Dyn;
  typedef Elf64_Sym Sym;
  typedef Elf64_Rel Rel;
  typedef Elf64_Rela Rela;
  typedef Elf64_Xword Relr;
  typedef Elf64_auxv_t auxv_t;

  enum { kElfClass = ELFCLASS64 };
  enum { kElfBits = 64 };

# ifndef ELF_R_TYPE
# define ELF_R_TYPE ELF64_R_TYPE
# endif

# ifndef ELF_R_SYM
# define ELF_R_SYM ELF64_R_SYM
# endif

// TODO(simonb): ELF64_R_INFO missing from the NDK.
# ifndef ELF64_R_INFO
# define ELF64_R_INFO(sym,type) ((((Elf64_Xword) (sym)) << 32) + (type))
# endif

# ifndef ELF_R_INFO
# define ELF_R_INFO ELF64_R_INFO
# endif
};
#else
#error "Unsupported target CPU bitness"
#endif

#ifdef __arm__
#define ELF_MACHINE EM_ARM
#elif defined(__i386__)
#define ELF_MACHINE EM_386
#elif defined(__x86_64__)
#define ELF_MACHINE EM_X86_64
#elif defined(__mips__) && !defined(__LP64__)  // mips64el defines __mips__ too
#define ELF_MACHINE EM_MIPS
#elif defined(__aarch64__)
#define ELF_MACHINE EM_AARCH64
#else
#error "Unsupported target CPU architecture"
#endif

#endif  // _ELF_TRAITS_H_
