// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crazy_linker_elf_relocations.h"

#include <assert.h>
#include <errno.h>
#include <memory>

#include "crazy_linker_debug.h"
#include "crazy_linker_elf_symbols.h"
#include "crazy_linker_elf_view.h"
#include "crazy_linker_error.h"
#include "crazy_linker_system.h"
#include "crazy_linker_util.h"
#include "linker_phdr.h"
#include "linker_reloc_iterators.h"
#include "linker_sleb128.h"

#define DEBUG_RELOCATIONS 0

#define RLOG(...) LOG_IF(DEBUG_RELOCATIONS, __VA_ARGS__)
#define RLOG_ERRNO(...) LOG_ERRNO_IF(DEBUG_RELOCATIONS, __VA_ARGS__)

#ifndef DF_SYMBOLIC
#define DF_SYMBOLIC 2
#endif

#ifndef DF_TEXTREL
#define DF_TEXTREL 4
#endif

#ifndef DT_FLAGS
#define DT_FLAGS 30
#endif

// Extension dynamic tags for Android packed relocations.
#ifndef DT_LOOS
#define DT_LOOS 0x6000000d
#endif
#ifndef DT_ANDROID_REL
#define DT_ANDROID_REL (DT_LOOS + 2)
#endif
#ifndef DT_ANDROID_RELSZ
#define DT_ANDROID_RELSZ (DT_LOOS + 3)
#endif
#ifndef DT_ANDROID_RELA
#define DT_ANDROID_RELA (DT_LOOS + 4)
#endif
#ifndef DT_ANDROID_RELASZ
#define DT_ANDROID_RELASZ (DT_LOOS + 5)
#endif

// Careful: the Android <elf.h> defines these with value corresponding to
// DT_ANDROID_RELRxx below, so we undefine them, just in case. The DT_RELRxx
// values corresponds to what lld generates by default with
// '--pack-dyn-relocs=relr'.
//
// For more details, see https://reviews.llvm.org/D48247
#undef DT_RELR
#define DT_RELR 0x24
#undef DT_RELRSZ
#define DT_RELRSZ 0x23
#undef DT_RELRENT
#define DT_RELRENT 0x25

// NOTE: The Android system linker only supports the DT_ANDROID_RELRxx entries
// but their content is exactly equivalent to the DT_RELRxx ones. One can tell
// lld to use the Android values with '--use-android-relr-tags'.
//
// The crazy linker supports both format, because it's essentially free :)
#ifndef DT_ANDROID_RELR
#define DT_ANDROID_RELR 0x6fffe000
#endif
#ifndef DT_ANDROID_RELRSZ
#define DT_ANDROID_RELRSZ 0x6fffe001
#endif
#ifndef DT_ANDROID_RELRENT
#define DT_ANDROID_RELRENT 0x6fffe003
#endif

// Processor-specific relocation types supported by the linker.
#ifdef __arm__

/* arm32 relocations */
#define R_ARM_ABS32 2
#define R_ARM_REL32 3
#define R_ARM_GLOB_DAT 21
#define R_ARM_JUMP_SLOT 22
#define R_ARM_COPY 20
#define R_ARM_RELATIVE 23

#define RELATIVE_RELOCATION_CODE R_ARM_RELATIVE

#endif  // __arm__

#ifdef __aarch64__

/* arm64 relocations */
#define R_AARCH64_ABS64 257
#define R_AARCH64_COPY 1024
#define R_AARCH64_GLOB_DAT 1025
#define R_AARCH64_JUMP_SLOT 1026
#define R_AARCH64_RELATIVE 1027

#define RELATIVE_RELOCATION_CODE R_AARCH64_RELATIVE

#endif  // __aarch64__

#ifdef __i386__

/* i386 relocations */
#define R_386_32 1
#define R_386_PC32 2
#define R_386_GLOB_DAT 6
#define R_386_JMP_SLOT 7
#define R_386_RELATIVE 8

#endif  // __i386__

#ifdef __x86_64__

/* x86_64 relocations */
#define R_X86_64_64 1
#define R_X86_64_PC32 2
#define R_X86_64_GLOB_DAT 6
#define R_X86_64_JMP_SLOT 7
#define R_X86_64_RELATIVE 8

#endif  // __x86_64__

namespace crazy {

namespace {

// List of known relocation types the relocator knows about.
enum RelocationType {
  RELOCATION_TYPE_UNKNOWN = 0,
  RELOCATION_TYPE_ABSOLUTE = 1,
  RELOCATION_TYPE_RELATIVE = 2,
  RELOCATION_TYPE_PC_RELATIVE = 3,
  RELOCATION_TYPE_COPY = 4,
};

// Convert an ELF relocation type info a RelocationType value.
RelocationType GetRelocationType(ELF::Word r_type) {
  switch (r_type) {
#ifdef __arm__
    case R_ARM_JUMP_SLOT:
    case R_ARM_GLOB_DAT:
    case R_ARM_ABS32:
      return RELOCATION_TYPE_ABSOLUTE;

    case R_ARM_REL32:
    case R_ARM_RELATIVE:
      return RELOCATION_TYPE_RELATIVE;

    case R_ARM_COPY:
      return RELOCATION_TYPE_COPY;
#endif

#ifdef __aarch64__
    case R_AARCH64_JUMP_SLOT:
    case R_AARCH64_GLOB_DAT:
    case R_AARCH64_ABS64:
      return RELOCATION_TYPE_ABSOLUTE;

    case R_AARCH64_RELATIVE:
      return RELOCATION_TYPE_RELATIVE;

    case R_AARCH64_COPY:
      return RELOCATION_TYPE_COPY;
#endif

#ifdef __i386__
    case R_386_JMP_SLOT:
    case R_386_GLOB_DAT:
    case R_386_32:
      return RELOCATION_TYPE_ABSOLUTE;

    case R_386_RELATIVE:
      return RELOCATION_TYPE_RELATIVE;

    case R_386_PC32:
      return RELOCATION_TYPE_PC_RELATIVE;
#endif

#ifdef __x86_64__
    case R_X86_64_JMP_SLOT:
    case R_X86_64_GLOB_DAT:
    case R_X86_64_64:
      return RELOCATION_TYPE_ABSOLUTE;

    case R_X86_64_RELATIVE:
      return RELOCATION_TYPE_RELATIVE;

    case R_X86_64_PC32:
      return RELOCATION_TYPE_PC_RELATIVE;
#endif

#ifdef __mips__
    case R_MIPS_REL32:
      return RELOCATION_TYPE_RELATIVE;
#endif

    default:
      return RELOCATION_TYPE_UNKNOWN;
  }
}

}  // namespace

ElfRelocations::ElfRelocations() = default;

bool ElfRelocations::Init(const ElfView* view, Error* error) {
  // Save these for later.
  phdr_ = view->phdr();
  phdr_count_ = view->phdr_count();
  load_bias_ = view->load_bias();

  // Parse the dynamic table.
  ElfView::DynamicIterator dyn(view);
  for (; dyn.HasNext(); dyn.GetNext()) {
    ELF::Addr dyn_value = dyn.GetValue();
    uintptr_t dyn_addr = dyn.GetAddress(view->load_bias());

    const ELF::Addr tag = dyn.GetTag();
    switch (tag) {
#if defined(USE_RELA)
      case DT_REL:
      case DT_RELSZ:
      case DT_ANDROID_REL:
      case DT_ANDROID_RELSZ:
#else
      case DT_RELA:
      case DT_RELASZ:
      case DT_ANDROID_RELA:
      case DT_ANDROID_RELASZ:
#endif
        LOG("unsupported relocation type");
        *error = "Relocation for wrong architecture";
        return false;
      case DT_PLTREL:
        RLOG("  DT_PLTREL value=%d\n", dyn_value);
#if defined(USE_RELA)
        if (dyn_value != DT_RELA) {
          RLOG("unsupported DT_PLTREL in \"%s\"; expected DT_RELA");
          return false;
        }
#else
        if (dyn_value != DT_REL) {
          RLOG("unsupported DT_PLTREL in \"%s\"; expected DT_REL");
          return false;
        }
#endif
        break;
      case DT_JMPREL:
        RLOG("  DT_JMPREL addr=%p\n", dyn_addr - load_bias_);
        plt_relocations_ = dyn_addr;
        break;
      case DT_PLTRELSZ:
        plt_relocations_size_ = dyn_value;
        RLOG("  DT_PLTRELSZ size=%d\n", dyn_value);
        break;
#if defined(USE_RELA)
      case DT_RELA:
#else
      case DT_REL:
#endif
        RLOG("  %s addr=%p\n", (tag == DT_RELA) ? "DT_RELA" : "DT_REL",
             dyn_addr - load_bias_);
        if (relocations_) {
          *error = "Unsupported DT_RELA/DT_REL combination in dynamic section";
          return false;
        }
        relocations_ = dyn_addr;
        break;
#if defined(USE_RELA)
      case DT_RELASZ:
#else
      case DT_RELSZ:
#endif
        RLOG("  %s size=%d\n", (tag == DT_RELASZ) ? "DT_RELASZ" : "DT_RELSZ",
             dyn_value);
        relocations_size_ = dyn_value;
        break;
#if defined(USE_RELA)
      case DT_ANDROID_RELA:
#else
      case DT_ANDROID_REL:
#endif
        RLOG("  %s addr=%p\n",
             (tag == DT_ANDROID_REL) ? "DT_ANDROID_REL" : "DT_ANDROID_RELA",
             dyn_addr - load_bias_);
        if (android_relocations_) {
          *error = "Multiple DT_ANDROID_* sections defined.";
          return false;
        }
        android_relocations_ = reinterpret_cast<uint8_t*>(dyn_addr);
        break;
#if defined(USE_RELA)
      case DT_ANDROID_RELASZ:
#else
      case DT_ANDROID_RELSZ:
#endif
        RLOG("  %s size=%d\n",
             (tag == DT_ANDROID_RELASZ) ? "DT_ANDROID_RELASZ"
                                        : "DT_ANDROID_RELSZ",
             dyn_value);
        android_relocations_size_ = dyn_value;
        break;
      case DT_RELR:
      case DT_ANDROID_RELR:
        RLOG("  DT_RELR\n");
        relr_.SetAddress(dyn_addr);
        break;
      case DT_ANDROID_RELRSZ:
      case DT_RELRSZ:
        relr_.SetSize(dyn_value);
        RLOG("  DT_RELSZ size=%d\n", dyn_value);
        break;
      case DT_RELRENT:
      case DT_ANDROID_RELRENT:
        if (dyn_value != sizeof(ELF::Relr)) {
          RLOG("Invalid RELR entry size (%d, expected %d)",
               static_cast<int>(dyn_value),
               static_cast<int>(sizeof(ELF::Relr)));
          *error = "Invalid DT_RELRENT value";
          return false;
        }
        break;
      case DT_PLTGOT:
        // Only used on MIPS currently. Could also be used on other platforms
        // when lazy binding (i.e. RTLD_LAZY) is implemented.
        RLOG("  DT_PLTGOT addr=%p\n", dyn_addr - load_bias_);
        plt_got_ = reinterpret_cast<ELF::Addr*>(dyn_addr);
        break;
      case DT_TEXTREL:
        RLOG("  DT_TEXTREL\n");
        has_text_relocations_ = true;
        break;
      case DT_SYMBOLIC:
        RLOG("  DT_SYMBOLIC\n");
        has_symbolic_ = true;
        break;
      case DT_FLAGS:
        if (dyn_value & DF_TEXTREL)
          has_text_relocations_ = true;
        if (dyn_value & DF_SYMBOLIC)
          has_symbolic_ = true;
        RLOG("  DT_FLAGS has_text_relocations=%s has_symbolic=%s\n",
             has_text_relocations_ ? "true" : "false",
             has_symbolic_ ? "true" : "false");
        break;
#if defined(__mips__)
      case DT_MIPS_SYMTABNO:
        RLOG("  DT_MIPS_SYMTABNO value=%d\n", dyn_value);
        mips_symtab_count_ = dyn_value;
        break;

      case DT_MIPS_LOCAL_GOTNO:
        RLOG("  DT_MIPS_LOCAL_GOTNO value=%d\n", dyn_value);
        mips_local_got_count_ = dyn_value;
        break;

      case DT_MIPS_GOTSYM:
        RLOG("  DT_MIPS_GOTSYM value=%d\n", dyn_value);
        mips_gotsym_ = dyn_value;
        break;
#endif
      default:
        ;
    }
  }

  return true;
}

bool ElfRelocations::ApplyAll(const ElfSymbols* symbols,
                              SymbolResolver* resolver,
                              Error* error) {
  LOG("Enter");

  if (has_text_relocations_) {
    if (phdr_table_unprotect_segments(phdr_, phdr_count_, load_bias_) < 0) {
      error->Format("Can't unprotect loadable segments: %s", strerror(errno));
      return false;
    }
  }

  if (!ApplyAndroidRelocations(symbols, resolver, error))
    return false;

  relr_.Apply(load_bias_);

  if (!ApplyRelocs(reinterpret_cast<rel_t*>(relocations_),
                   relocations_size_ / sizeof(rel_t), symbols, resolver, error))
    return false;
  if (!ApplyRelocs(reinterpret_cast<rel_t*>(plt_relocations_),
                   plt_relocations_size_ / sizeof(rel_t), symbols, resolver,
                   error))
    return false;

#ifdef __mips__
  if (!RelocateMipsGot(symbols, resolver, error))
    return false;
#endif

  if (has_text_relocations_) {
    if (phdr_table_protect_segments(phdr_, phdr_count_, load_bias_) < 0) {
      error->Format("Can't reprotect loadable segments: %s", strerror(errno));
      return false;
    }
  }

  LOG("Done");
  return true;
}

// Helper class for Android packed relocations.  Encapsulates the packing
// flags used by Android for packed relocation groups.
class AndroidPackedRelocationGroupFlags {
 public:
  explicit AndroidPackedRelocationGroupFlags(size_t flags) : flags_(flags) { }

  bool is_relocation_grouped_by_info() const {
    return hasFlag(kRelocationGroupedByInfoFlag);
  }
  bool is_relocation_grouped_by_offset_delta() const {
    return hasFlag(kRelocationGroupedByOffsetDeltaFlag);
  }
  bool is_relocation_grouped_by_addend() const {
    return hasFlag(kRelocationGroupedByAddendFlag);
  }
  bool is_relocation_group_has_addend() const {
    return hasFlag(kRelocationGroupHasAddendFlag);
  }

 private:
  bool hasFlag(size_t flag) const { return (flags_ & flag) != 0; }

  static const size_t kRelocationGroupedByInfoFlag = 1 << 0;
  static const size_t kRelocationGroupedByOffsetDeltaFlag = 1 << 1;
  static const size_t kRelocationGroupedByAddendFlag = 1 << 2;
  static const size_t kRelocationGroupHasAddendFlag = 1 << 3;

  const size_t flags_;
};

template <typename ElfRelIteratorT>
bool ElfRelocations::ForEachAndroidRelocationHelper(
    ElfRelIteratorT&& rel_iterator,
    RelocationHandler handler,
    void* opaque) {
  size_t relocations_handled = 0;
  while (rel_iterator.has_next()) {
    const auto rel = rel_iterator.next();
    if (rel == nullptr) {
      LOG("failed to parse relocation %d", relocations_handled);
      return false;
    }
    // Pass the relocation to the supplied handler function. If the handler
    // returns false we view this as failure and return false to our caller.
    if (!handler(this, rel, opaque)) {
      LOG("failed handling relocation %d", relocations_handled);
      return false;
    }
    relocations_handled++;
  }
  LOG("relocations_handled=%d", relocations_handled);
  return true;
}

bool ElfRelocations::ForEachAndroidRelocation(RelocationHandler handler,
                                              void* opaque) {
  // Skip over the "APS2" signature.
  const uint8_t* packed_relocs = android_relocations_ + 4;
  const size_t packed_relocs_size = android_relocations_size_ - 4;
  return ForEachAndroidRelocationHelper(
      packed_reloc_iterator<sleb128_decoder>(
          sleb128_decoder(packed_relocs, packed_relocs_size)),
      handler, opaque);
}

namespace {

// Validate the Android packed relocations signature.
bool IsValidAndroidPackedRelocations(const uint8_t* android_relocations,
                                     size_t android_relocations_size) {
  if (android_relocations_size < 4)
    return false;

  // Check for an initial APS2 Android packed relocations header.
  return (android_relocations[0] == 'A' &&
          android_relocations[1] == 'P' &&
          android_relocations[2] == 'S' &&
          android_relocations[3] == '2');
}

}  // namespace

// Args for ApplyAndroidRelocation handler function.
struct ApplyAndroidRelocationArgs {
  const ElfSymbols* symbols;
  ElfRelocations::SymbolResolver* resolver;
  Error* error;
};

// Static ForEachAndroidRelocation() handler.
bool ElfRelocations::ApplyAndroidRelocation(ElfRelocations* relocations,
                                            const rel_t* relocation,
                                            void* opaque) {
  // Unpack args from opaque.
  ApplyAndroidRelocationArgs* args =
      reinterpret_cast<ApplyAndroidRelocationArgs*>(opaque);
  const ElfSymbols* symbols = args->symbols;
  ElfRelocations::SymbolResolver* resolver = args->resolver;
  Error* error = args->error;

  return relocations->ApplyReloc(relocation, symbols, resolver, error);
}

bool ElfRelocations::ApplyAndroidRelocations(const ElfSymbols* symbols,
                                             SymbolResolver* resolver,
                                             Error* error) {
  if (!android_relocations_)
    return true;

  if (!IsValidAndroidPackedRelocations(android_relocations_,
                                       android_relocations_size_))
    return false;

  ApplyAndroidRelocationArgs args;
  args.symbols = symbols;
  args.resolver = resolver;
  args.error = error;
  return ForEachAndroidRelocation(&ApplyAndroidRelocation, &args);
}

#if defined(USE_RELA)
bool ElfRelocations::ApplyResolvedReloc(const ELF::Rela* rela,
                                        ELF::Addr sym_addr,
                                        bool resolved CRAZY_UNUSED,
                                        Error* error) {
  const ELF::Word rela_type = ELF_R_TYPE(rela->r_info);
  const ELF::Word CRAZY_UNUSED rela_symbol = ELF_R_SYM(rela->r_info);
  const ELF::Sword CRAZY_UNUSED addend = rela->r_addend;

  const ELF::Addr reloc = static_cast<ELF::Addr>(rela->r_offset + load_bias_);

  RLOG("  rela reloc=%p offset=%p type=%d addend=%p\n",
       reloc,
       rela->r_offset,
       rela_type,
       addend);

  // Apply the relocation.
  ELF::Addr* CRAZY_UNUSED target = reinterpret_cast<ELF::Addr*>(reloc);
  switch (rela_type) {
#ifdef __aarch64__
    case R_AARCH64_JUMP_SLOT:
      RLOG("  R_AARCH64_JUMP_SLOT target=%p addr=%p\n",
           target,
           sym_addr + addend);
      *target = sym_addr + addend;
      break;

    case R_AARCH64_GLOB_DAT:
      RLOG("  R_AARCH64_GLOB_DAT target=%p addr=%p\n",
           target,
           sym_addr + addend);
      *target = sym_addr + addend;
      break;

    case R_AARCH64_ABS64:
      RLOG("  R_AARCH64_ABS64 target=%p (%p) addr=%p\n",
           target,
           *target,
           sym_addr + addend);
      *target += sym_addr + addend;
      break;

    case R_AARCH64_RELATIVE:
      RLOG("  R_AARCH64_RELATIVE target=%p (%p) bias=%p\n",
           target,
           *target,
           load_bias_ + addend);
      if (__builtin_expect(rela_symbol, 0)) {
        *error = "Invalid relative relocation with symbol";
        return false;
      }
      *target = load_bias_ + addend;
      break;

    case R_AARCH64_COPY:
      // NOTE: These relocations are forbidden in shared libraries.
      RLOG("  R_AARCH64_COPY\n");
      *error = "Invalid R_AARCH64_COPY relocation in shared library";
      return false;
#endif  // __aarch64__

#ifdef __x86_64__
    case R_X86_64_JMP_SLOT:
      *target = sym_addr + addend;
      break;

    case R_X86_64_GLOB_DAT:
      *target = sym_addr + addend;
      break;

    case R_X86_64_RELATIVE:
      if (rela_symbol) {
        *error = "Invalid relative relocation with symbol";
        return false;
      }
      *target = load_bias_ + addend;
      break;

    case R_X86_64_64:
      *target = sym_addr + addend;
      break;

    case R_X86_64_PC32:
      *target = sym_addr + (addend - reloc);
      break;
#endif  // __x86_64__

    default:
      error->Format("Invalid relocation type (%d)", rela_type);
      return false;
  }

  return true;
}

#else

bool ElfRelocations::ApplyResolvedReloc(const ELF::Rel* rel,
                                        ELF::Addr sym_addr,
                                        bool resolved CRAZY_UNUSED,
                                        Error* error) {
  const ELF::Word rel_type = ELF_R_TYPE(rel->r_info);
  const ELF::Word CRAZY_UNUSED rel_symbol = ELF_R_SYM(rel->r_info);

  const ELF::Addr reloc = static_cast<ELF::Addr>(rel->r_offset + load_bias_);

  RLOG("  rel reloc=%p offset=%p type=%d\n", reloc, rel->r_offset, rel_type);

  // Apply the relocation.
  ELF::Addr* CRAZY_UNUSED target = reinterpret_cast<ELF::Addr*>(reloc);
  switch (rel_type) {
#ifdef __arm__
    case R_ARM_JUMP_SLOT:
      RLOG("  R_ARM_JUMP_SLOT target=%p addr=%p\n", target, sym_addr);
      *target = sym_addr;
      break;

    case R_ARM_GLOB_DAT:
      RLOG("  R_ARM_GLOB_DAT target=%p addr=%p\n", target, sym_addr);
      *target = sym_addr;
      break;

    case R_ARM_ABS32:
      RLOG("  R_ARM_ABS32 target=%p (%p) addr=%p\n",
           target,
           *target,
           sym_addr);
      *target += sym_addr;
      break;

    case R_ARM_REL32:
      RLOG("  R_ARM_REL32 target=%p (%p) addr=%p offset=%p\n",
           target,
           *target,
           sym_addr,
           rel->r_offset);
      *target += sym_addr - rel->r_offset;
      break;

    case R_ARM_RELATIVE:
      RLOG("  R_ARM_RELATIVE target=%p (%p) bias=%p\n",
           target,
           *target,
           load_bias_);
      if (__builtin_expect(rel_symbol, 0)) {
        *error = "Invalid relative relocation with symbol";
        return false;
      }
      *target += load_bias_;
      break;

    case R_ARM_COPY:
      // NOTE: These relocations are forbidden in shared libraries.
      // The Android linker has special code to deal with this, which
      // is not needed here.
      RLOG("  R_ARM_COPY\n");
      *error = "Invalid R_ARM_COPY relocation in shared library";
      return false;
#endif  // __arm__

#ifdef __i386__
    case R_386_JMP_SLOT:
      *target = sym_addr;
      break;

    case R_386_GLOB_DAT:
      *target = sym_addr;
      break;

    case R_386_RELATIVE:
      if (rel_symbol) {
        *error = "Invalid relative relocation with symbol";
        return false;
      }
      *target += load_bias_;
      break;

    case R_386_32:
      *target += sym_addr;
      break;

    case R_386_PC32:
      *target += (sym_addr - reloc);
      break;
#endif  // __i386__

#ifdef __mips__
    case R_MIPS_REL32:
      if (resolved)
        *target += sym_addr;
      else
        *target += load_bias_;
      break;
#endif  // __mips__

    default:
      error->Format("Invalid relocation type (%d)", rel_type);
      return false;
  }

  return true;
}
#endif  // defined(USE_RELA)

bool ElfRelocations::ResolveSymbol(ELF::Word rel_type,
                                   ELF::Word rel_symbol,
                                   const ElfSymbols* symbols,
                                   SymbolResolver* resolver,
                                   ELF::Addr reloc,
                                   ELF::Addr* sym_addr,
                                   Error* error) {
  const char* sym_name = symbols->LookupNameById(rel_symbol);
  RLOG("    symbol name='%s'\n", sym_name);
  void* address = resolver->Lookup(sym_name);

  if (address) {
    // The symbol was found, so compute its address.
    RLOG("symbol %s resolved to %p", sym_name, address);
    *sym_addr = reinterpret_cast<ELF::Addr>(address);
    return true;
  }

  // The symbol was not found. Normally this is an error except
  // if this is a weak reference.
  if (!symbols->IsWeakById(rel_symbol)) {
    error->Format("Could not find symbol '%s'", sym_name);
    return false;
  }

  RLOG("weak reference to unresolved symbol %s", sym_name);

  // IHI0044C AAELF 4.5.1.1:
  // Libraries are not searched to resolve weak references.
  // It is not an error for a weak reference to remain
  // unsatisfied.
  //
  // During linking, the value of an undefined weak reference is:
  // - Zero if the relocation type is absolute
  // - The address of the place if the relocation is pc-relative
  // - The address of nominal base address if the relocation
  //   type is base-relative.
  RelocationType r = GetRelocationType(rel_type);
  if (r == RELOCATION_TYPE_ABSOLUTE || r == RELOCATION_TYPE_RELATIVE) {
    *sym_addr = 0;
    return true;
  }

  if (r == RELOCATION_TYPE_PC_RELATIVE) {
    *sym_addr = reloc;
    return true;
  }

  error->Format(
      "Invalid weak relocation type (%d) for unknown symbol '%s'",
      r,
      sym_name);
  return false;
}

bool ElfRelocations::ApplyReloc(const rel_t* rel,
                                const ElfSymbols* symbols,
                                SymbolResolver* resolver,
                                Error* error) {
  const ELF::Word rel_type = ELF_R_TYPE(rel->r_info);
  const ELF::Word rel_symbol = ELF_R_SYM(rel->r_info);

  ELF::Addr sym_addr = 0;
  ELF::Addr reloc = static_cast<ELF::Addr>(rel->r_offset + load_bias_);
  RLOG("  offset=%p type=%d reloc=%p symbol=%d\n", rel->r_offset, rel_type,
       reloc, rel_symbol);

  if (rel_type == 0)
    return true;

  bool resolved = false;

  // If this is a symbolic relocation, compute the symbol's address.
  if (__builtin_expect(rel_symbol != 0, 0)) {
    if (!ResolveSymbol(rel_type,
                       rel_symbol,
                       symbols,
                       resolver,
                       reloc,
                       &sym_addr,
                       error)) {
      return false;
    }
    resolved = true;
  }

  return ApplyResolvedReloc(rel, sym_addr, resolved, error);
}

bool ElfRelocations::ApplyRelocs(const rel_t* rel,
                                 size_t rel_count,
                                 const ElfSymbols* symbols,
                                 SymbolResolver* resolver,
                                 Error* error) {
  RLOG("rel=%p rel_count=%d", rel, rel_count);

  if (!rel)
    return true;

  for (size_t rel_n = 0; rel_n < rel_count; rel++, rel_n++) {
    RLOG("  Relocation %d of %d:\n", rel_n + 1, rel_count);

    if (!ApplyReloc(rel, symbols, resolver, error))
      return false;
  }

  return true;
}

#ifdef __mips__
bool ElfRelocations::RelocateMipsGot(const ElfSymbols* symbols,
                                     SymbolResolver* resolver,
                                     Error* error) {
  if (!plt_got_)
    return true;

  // Handle the local GOT entries.
  // This mimics what the system linker does.
  // Note from the system linker:
  // got[0]: lazy resolver function address.
  // got[1]: may be used for a GNU extension.
  // Set it to a recognizable address in case someone calls it
  // (should be _rtld_bind_start).
  ELF::Addr* got = plt_got_;
  got[0] = 0xdeadbeef;
  if (got[1] & 0x80000000)
    got[1] = 0xdeadbeef;

  for (ELF::Addr n = 2; n < mips_local_got_count_; ++n)
    got[n] += load_bias_;

  // Handle the global GOT entries.
  got += mips_local_got_count_;
  for (size_t idx = mips_gotsym_; idx < mips_symtab_count_; idx++, got++) {
    const char* sym_name = symbols->LookupNameById(idx);
    void* sym_addr = resolver->Lookup(sym_name);
    if (sym_addr) {
      // Found symbol, update GOT entry.
      *got = reinterpret_cast<ELF::Addr>(sym_addr);
      continue;
    }

    if (symbols->IsWeakById(idx)) {
      // Undefined symbols are only ok if this is a weak reference.
      // Update GOT entry to 0 though.
      *got = 0;
      continue;
    }

    error->Format("Cannot locate symbol %s", sym_name);
    return false;
  }

  return true;
}
#endif  // __mips__

void ElfRelocations::AdjustRelocation(ELF::Word rel_type,
                                      ELF::Addr src_reloc,
                                      size_t dst_delta,
                                      size_t map_delta) {
  ELF::Addr* dst_ptr = reinterpret_cast<ELF::Addr*>(src_reloc + dst_delta);

  switch (rel_type) {
#ifdef __arm__
    case R_ARM_RELATIVE:
      *dst_ptr += map_delta;
      break;
#endif  // __arm__

#ifdef __aarch64__
    case R_AARCH64_RELATIVE:
      *dst_ptr += map_delta;
      break;
#endif  // __aarch64__

#ifdef __i386__
    case R_386_RELATIVE:
      *dst_ptr += map_delta;
      break;
#endif

#ifdef __x86_64__
    case R_X86_64_RELATIVE:
      *dst_ptr += map_delta;
      break;
#endif

#ifdef __mips__
    case R_MIPS_REL32:
      *dst_ptr += map_delta;
      break;
#endif
    default:
      ;
  }
}

void ElfRelocations::AdjustAndroidRelocation(const rel_t* relocation,
                                             size_t src_addr,
                                             size_t dst_addr,
                                             size_t map_addr,
                                             size_t size) {
  // Add this value to each source address to get the corresponding
  // destination address.
  const size_t dst_delta = dst_addr - src_addr;
  const size_t map_delta = map_addr - src_addr;

  const ELF::Word rel_type = ELF_R_TYPE(relocation->r_info);
  const ELF::Word rel_symbol = ELF_R_SYM(relocation->r_info);
  ELF::Addr src_reloc =
      static_cast<ELF::Addr>(relocation->r_offset + load_bias_);

  if (rel_type == 0 || rel_symbol != 0) {
    // Ignore empty and symbolic relocations
    return;
  }

  if (src_reloc < src_addr || src_reloc >= src_addr + size) {
    // Ignore entries that don't relocate addresses inside the source section.
    return;
  }

  AdjustRelocation(rel_type, src_reloc, dst_delta, map_delta);
}

// Args for ApplyAndroidRelocation handler function.
struct RelocateAndroidRelocationArgs {
  size_t src_addr;
  size_t dst_addr;
  size_t map_addr;
  size_t size;
};

// Static ForEachAndroidRelocation() handler.
bool ElfRelocations::RelocateAndroidRelocation(ElfRelocations* relocations,
                                               const rel_t* relocation,
                                               void* opaque) {
  // Unpack args from opaque, to obtain addrs and size;
  RelocateAndroidRelocationArgs* args =
      reinterpret_cast<RelocateAndroidRelocationArgs*>(opaque);
  const size_t src_addr = args->src_addr;
  const size_t dst_addr = args->dst_addr;
  const size_t map_addr = args->map_addr;
  const size_t size = args->size;

  relocations->AdjustAndroidRelocation(relocation,
                                       src_addr,
                                       dst_addr,
                                       map_addr,
                                       size);
  return true;
}

void ElfRelocations::RelocateAndroidRelocations(size_t src_addr,
                                                size_t dst_addr,
                                                size_t map_addr,
                                                size_t size) {
  if (!android_relocations_)
    return;

  assert(IsValidAndroidPackedRelocations(android_relocations_,
                                         android_relocations_size_));

  RelocateAndroidRelocationArgs args;
  args.src_addr = src_addr;
  args.dst_addr = dst_addr;
  args.map_addr = map_addr;
  args.size = size;
  ForEachAndroidRelocation(&RelocateAndroidRelocation, &args);
}

void ElfRelocations::RelocateRelocations(size_t src_addr,
                                         size_t dst_addr,
                                         size_t map_addr,
                                         size_t size) {
  // Add this value to each source address to get the corresponding
  // destination address.
  const size_t dst_delta = dst_addr - src_addr;
  const size_t map_delta = map_addr - src_addr;

  // Ignore PLT relocations, which all target symbols (ignored here).
  const rel_t* rel = reinterpret_cast<rel_t*>(relocations_);
  const size_t relocations_count = relocations_size_ / sizeof(rel_t);
  const rel_t* rel_limit = rel + relocations_count;

  for (; rel < rel_limit; ++rel) {
    const ELF::Word rel_type = ELF_R_TYPE(rel->r_info);
    const ELF::Word rel_symbol = ELF_R_SYM(rel->r_info);
    ELF::Addr src_reloc = static_cast<ELF::Addr>(rel->r_offset + load_bias_);

    if (rel_type == 0 || rel_symbol != 0) {
      // Ignore empty and symbolic relocations
      continue;
    }

    if (src_reloc < src_addr || src_reloc >= src_addr + size) {
      // Ignore entries that don't relocate addresses inside the source section.
      continue;
    }

    AdjustRelocation(rel_type, src_reloc, dst_delta, map_delta);
  }
}

void ElfRelocations::CopyAndRelocate(size_t src_addr,
                                     size_t dst_addr,
                                     size_t map_addr,
                                     size_t size) {
  // First, a straight copy.
  ::memcpy(reinterpret_cast<void*>(dst_addr),
           reinterpret_cast<void*>(src_addr),
           size);

  // Relocate android relocations.
  RelocateAndroidRelocations(src_addr, dst_addr, map_addr, size);

  // Relocate relocations.
  RelocateRelocations(src_addr, dst_addr, map_addr, size);

#ifdef __mips__
  // Add this value to each source address to get the corresponding
  // destination address.
  const size_t dst_delta = dst_addr - src_addr;
  const size_t map_delta = map_addr - src_addr;

  // Only relocate local GOT entries.
  ELF::Addr* got = plt_got_;
  if (got) {
    for (ELF::Addr n = 2; n < mips_local_got_count_; ++n) {
      size_t got_addr = reinterpret_cast<size_t>(&got[n]);
      if (got_addr < src_addr || got_addr >= src_addr + size)
        continue;
      ELF::Addr* dst_ptr = reinterpret_cast<ELF::Addr*>(got_addr + dst_delta);
      *dst_ptr += map_delta;
    }
  }
#endif
}

}  // namespace crazy
