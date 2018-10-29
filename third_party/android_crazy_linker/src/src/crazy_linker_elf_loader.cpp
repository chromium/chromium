// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crazy_linker_elf_loader.h"

#include <limits.h>  // For PAGE_SIZE and PAGE_MASK

#include "crazy_linker_debug.h"
#include "linker_phdr.h"

#define PAGE_START(x) ((x) & PAGE_MASK)
#define PAGE_OFFSET(x) ((x) & ~PAGE_MASK)
#define PAGE_END(x) PAGE_START((x) + (PAGE_SIZE - 1))

namespace crazy {

#define MAYBE_MAP_FLAG(x, from, to) (((x) & (from)) ? (to) : 0)
#define PFLAGS_TO_PROT(x)                 \
  (MAYBE_MAP_FLAG((x), PF_X, PROT_EXEC) | \
   MAYBE_MAP_FLAG((x), PF_R, PROT_READ) | \
   MAYBE_MAP_FLAG((x), PF_W, PROT_WRITE))

// Whether to add a Breakpad guard region.  Breakpad currently requires
// a library's reported start_addr to be equal to its load_bias.  It will
// also complain if there is an apparent overlap in the memory mappings
// it reads in from a microdump.  So if load_bias and start_addr differ
// due to relocation packing, and if something in the process mmaps into
// the address space between load_bias and start_addr, this will break
// Breakpad's minidump processor.
//
// For now, the expedient workround is to add a "guard region" ahead of
// the start_addr where a library with a non-zero min vaddr is loaded.
// Making this part of the reserved address space for the library ensures
// that nothing will later mmap into it.
//
// Note: ~SharedLibrary() calls munmap() on the values returned from here
// through load_start() and load_size().  Where we added a guard region,
// these values do not cover the entire reserved mapping.  The result is
// that we potentially 'leak' between 2Mb and 6Mb of virtual address space
// if we unload a library in a controlled fashion.  For now we live with
// this, since a) we do not unload libraries on Android targets, and b) any
// leakage that might occur if we did is small and should not consume any
// real memory.
//
// Also defined in linker_jni.cc.  If changing here, change there also.
//
// For more, see:
//   https://crbug.com/504410
#define RESERVE_BREAKPAD_GUARD_REGION 1

namespace {

class InternalElfLoader {
 public:
  ~InternalElfLoader();

  bool LoadAt(const char* lib_path,
              off_t file_offset,
              uintptr_t wanted_address,
              Error* error);

  // Only call the following functions after a successful LoadAt() call.

  size_t phdr_count() { return phdr_num_; }
  ELF::Addr load_start() { return reinterpret_cast<ELF::Addr>(load_start_); }
  ELF::Addr load_size() { return load_size_; }
  ELF::Addr load_bias() { return load_bias_; }
  const ELF::Phdr* loaded_phdr() { return loaded_phdr_; }

  // Return the mapping object covering the reserved address space for this
  // ELF object. Caller takes ownership.
  MemoryMapping ReleaseMapping() { return std::move(reserved_map_); }

 private:
  FileDescriptor fd_;
  const char* path_ = nullptr;

  ELF::Ehdr header_ = {};
  size_t phdr_num_ = 0;

  void* phdr_mmap_ = nullptr;  // temporary copy of the program header.
  ELF::Phdr* phdr_table_ = nullptr;
  ELF::Addr phdr_size_ = 0;  // and its size.

  off_t file_offset_ = 0;
  void* wanted_load_address_ = nullptr;
  void* load_start_ = nullptr;  // First page of reserved address space.
  ELF::Addr load_size_ = 0;     // Size in bytes of reserved address space.
  ELF::Addr load_bias_ = 0;     // load_bias, add this value to all "vaddr"
                             // values in the library to get the corresponding
                             // memory address.

  const ELF::Phdr* loaded_phdr_ =
      nullptr;  // points to the loaded program header.

  MemoryMapping reserved_map_;

  // Individual steps used by ::LoadAt()
  bool ReadElfHeader(Error* error);
  bool ReadProgramHeader(Error* error);
  bool ReserveAddressSpace(Error* error);
  bool LoadSegments(Error* error);
  bool FindPhdr(Error* error);
  bool CheckPhdr(ELF::Addr, Error* error);
};

InternalElfLoader::~InternalElfLoader() {
  if (phdr_mmap_) {
    // Deallocate the temporary program header copy.
    munmap(phdr_mmap_, phdr_size_);
  }
}

bool InternalElfLoader::LoadAt(const char* lib_path,
                               off_t file_offset,
                               uintptr_t wanted_address,
                               Error* error) {
  LOG("lib_path='%s', file_offset=%p, load_address=%p", lib_path, file_offset,
      wanted_address);

  // Check that the load address is properly page-aligned.
  if (wanted_address != PAGE_START(wanted_address)) {
    error->Format("Load address is not page aligned (%08x)", wanted_address);
    return false;
  }
  wanted_load_address_ = reinterpret_cast<void*>(wanted_address);

  // Check that the file offset is also properly page-aligned.
  // PAGE_START() can't be used here due to the compiler complaining about
  // comparing signed (off_t) and unsigned (size_t) values.
  if ((file_offset & static_cast<off_t>(PAGE_SIZE - 1)) != 0) {
    error->Format("File offset is not page aligned (%08x)", file_offset);
    return false;
  }
  file_offset_ = file_offset;

  // Open the file.
  if (!fd_.OpenReadOnly(lib_path)) {
    error->Format("Can't open file: %s", strerror(errno));
    return false;
  }

  if (file_offset && fd_.SeekTo(file_offset) < 0) {
    error->Format(
        "Can't seek to file offset %08x: %s", file_offset, strerror(errno));
    return false;
  }

  path_ = lib_path;

  if (!ReadElfHeader(error) || !ReadProgramHeader(error) ||
      !ReserveAddressSpace(error)) {
    return false;
  }

  if (!LoadSegments(error) || !FindPhdr(error)) {
    // An error occured, cleanup the address space by un-mapping the
    // range that was reserved by ReserveAddressSpace().
    reserved_map_.Deallocate();
    return false;
  }

  return true;
}

bool InternalElfLoader::ReadElfHeader(Error* error) {
  int ret = fd_.Read(&header_, sizeof(header_));
  if (ret < 0) {
    error->Format("Can't read file: %s", strerror(errno));
    return false;
  }
  if (ret != static_cast<int>(sizeof(header_))) {
    error->Set("File too small to be ELF");
    return false;
  }

  if (memcmp(header_.e_ident, ELFMAG, SELFMAG) != 0) {
    error->Set("Bad ELF magic");
    return false;
  }

  if (header_.e_ident[EI_CLASS] != ELF::kElfClass) {
    error->Format("Not a %d-bit class: %d",
                  ELF::kElfBits,
                  header_.e_ident[EI_CLASS]);
    return false;
  }

  if (header_.e_ident[EI_DATA] != ELFDATA2LSB) {
    error->Format("Not little-endian class: %d", header_.e_ident[EI_DATA]);
    return false;
  }

  if (header_.e_type != ET_DYN) {
    error->Format("Not a shared library type: %d", header_.e_type);
    return false;
  }

  if (header_.e_version != EV_CURRENT) {
    error->Format("Unexpected ELF version: %d", header_.e_version);
    return false;
  }

  if (header_.e_machine != ELF_MACHINE) {
    error->Format("Unexpected ELF machine type: %d", header_.e_machine);
    return false;
  }

  return true;
}

// Loads the program header table from an ELF file into a read-only private
// anonymous mmap-ed block.
bool InternalElfLoader::ReadProgramHeader(Error* error) {
  phdr_num_ = header_.e_phnum;

  // Like the kernel, only accept program header tables smaller than 64 KB.
  if (phdr_num_ < 1 || phdr_num_ > 65536 / sizeof(ELF::Phdr)) {
    error->Format("Invalid program header count: %d", phdr_num_);
    return false;
  }

  ELF::Addr page_min = PAGE_START(header_.e_phoff);
  ELF::Addr page_max =
      PAGE_END(header_.e_phoff + (phdr_num_ * sizeof(ELF::Phdr)));
  ELF::Addr page_offset = PAGE_OFFSET(header_.e_phoff);

  phdr_size_ = page_max - page_min;

  void* mmap_result = fd_.Map(
      NULL, phdr_size_, PROT_READ, MAP_PRIVATE, page_min + file_offset_);
  if (!mmap_result) {
    error->Format("Phdr mmap failed: %s", strerror(errno));
    return false;
  }

  phdr_mmap_ = mmap_result;
  phdr_table_ = reinterpret_cast<ELF::Phdr*>(
      reinterpret_cast<char*>(mmap_result) + page_offset);
  return true;
}

// Reserve a virtual address range big enough to hold all loadable
// segments of a program header table. This is done by creating a
// private anonymous mmap() with PROT_NONE.
//
// This will use the wanted_load_address_ value. Fails if the requested
// address range cannot be reserved. Typically this would be because
// it overlaps an existing, possibly system, mapping.
bool InternalElfLoader::ReserveAddressSpace(Error* error) {
  ELF::Addr min_vaddr;
  load_size_ =
      phdr_table_get_load_size(phdr_table_, phdr_num_, &min_vaddr, NULL);
  if (load_size_ == 0) {
    error->Set("No loadable segments");
    return false;
  }

  uint8_t* addr = NULL;
  int mmap_flags = MAP_PRIVATE | MAP_ANONYMOUS;

  // Support loading at a fixed address.
  if (wanted_load_address_) {
    addr = static_cast<uint8_t*>(wanted_load_address_);
  }

  size_t reserved_size = load_size_;

#if RESERVE_BREAKPAD_GUARD_REGION
  // Increase size to extend the address reservation mapping so that it will
  // also include a min_vaddr bytes region from load_bias_ to start_addr.
  // If loading at a fixed address, move our requested address back by the
  // guard region size.
  if (min_vaddr) {
    reserved_size += min_vaddr;
    if (wanted_load_address_) {
      addr -= min_vaddr;
    }
    LOG("added %d to size, for Breakpad guard", min_vaddr);
  }
#endif

  LOG("address=%p size=%p", addr, reserved_size);
  void* start = mmap(addr, reserved_size, PROT_NONE, mmap_flags, -1, 0);
  if (start == MAP_FAILED) {
    error->Format("Could not reserve %d bytes of address space", reserved_size);
    return false;
  }
  if (addr && start != addr) {
    error->Format("Could not map at %p requested, backing out", addr);
    munmap(start, reserved_size);
    return false;
  }

  // Take ownership of the mapping here.
  reserved_map_ = MemoryMapping(start, reserved_size);
  LOG("reserved start=%p", reserved_map_.address());

  load_start_ = start;
  load_bias_ = reinterpret_cast<ELF::Addr>(start) - min_vaddr;

#if RESERVE_BREAKPAD_GUARD_REGION
  // If we increased size to accommodate a Breakpad guard region, move
  // load_start_ and load_bias_ upwards by the size of the guard region.
  // File data is mapped at load_start_, and while nothing ever uses the
  // guard region between load_bias_ and load_start_, the fact that we
  // included it in the mmap() above means that nothing else will map it.
  if (min_vaddr) {
    load_start_ = reinterpret_cast<void*>(
        reinterpret_cast<uintptr_t>(load_start_) + min_vaddr);
    load_bias_ += min_vaddr;
    LOG("moved map by %d, for Breakpad guard", min_vaddr);
  }
#endif

  LOG("load start=%p, bias=%p", load_start_, load_bias_);
  return true;
}

// Returns the address of the program header table as it appears in the loaded
// segments in memory. This is in contrast with 'phdr_table_' which
// is temporary and will be released before the library is relocated.
bool InternalElfLoader::FindPhdr(Error* error) {
  const ELF::Phdr* phdr_limit = phdr_table_ + phdr_num_;

  // If there is a PT_PHDR, use it directly.
  for (const ELF::Phdr* phdr = phdr_table_; phdr < phdr_limit; ++phdr) {
    if (phdr->p_type == PT_PHDR) {
      return CheckPhdr(load_bias_ + phdr->p_vaddr, error);
    }
  }

  // Otherwise, check the first loadable segment. If its file offset
  // is 0, it starts with the ELF header, and we can trivially find the
  // loaded program header from it.
  for (const ELF::Phdr* phdr = phdr_table_; phdr < phdr_limit; ++phdr) {
    if (phdr->p_type == PT_LOAD) {
      if (phdr->p_offset == 0) {
        ELF::Addr elf_addr = load_bias_ + phdr->p_vaddr;
        const ELF::Ehdr* ehdr = (const ELF::Ehdr*)(void*)elf_addr;
        ELF::Addr offset = ehdr->e_phoff;
        return CheckPhdr((ELF::Addr)ehdr + offset, error);
      }
      break;
    }
  }

  error->Set("Can't find loaded program header");
  return false;
}

// Ensures that our program header is actually within a loadable
// segment. This should help catch badly-formed ELF files that
// would cause the linker to crash later when trying to access it.
bool InternalElfLoader::CheckPhdr(ELF::Addr loaded, Error* error) {
  const ELF::Phdr* phdr_limit = phdr_table_ + phdr_num_;
  ELF::Addr loaded_end = loaded + (phdr_num_ * sizeof(ELF::Phdr));
  for (ELF::Phdr* phdr = phdr_table_; phdr < phdr_limit; ++phdr) {
    if (phdr->p_type != PT_LOAD) {
      continue;
    }
    ELF::Addr seg_start = phdr->p_vaddr + load_bias_;
    ELF::Addr seg_end = phdr->p_filesz + seg_start;
    if (seg_start <= loaded && loaded_end <= seg_end) {
      loaded_phdr_ = reinterpret_cast<const ELF::Phdr*>(loaded);
      return true;
    }
  }
  error->Format("Loaded program header %x not in loadable segment", loaded);
  return false;
}

// Map all loadable segments in process' address space.
// This assumes you already called phdr_table_reserve_memory to
// reserve the address space range for the library.
bool InternalElfLoader::LoadSegments(Error* error) {
  for (size_t i = 0; i < phdr_num_; ++i) {
    const ELF::Phdr* phdr = &phdr_table_[i];

    if (phdr->p_type != PT_LOAD) {
      continue;
    }

    // Segment addresses in memory.
    ELF::Addr seg_start = phdr->p_vaddr + load_bias_;
    ELF::Addr seg_end = seg_start + phdr->p_memsz;

    ELF::Addr seg_page_start = PAGE_START(seg_start);
    ELF::Addr seg_page_end = PAGE_END(seg_end);

    ELF::Addr seg_file_end = seg_start + phdr->p_filesz;

    // File offsets.
    ELF::Addr file_start = phdr->p_offset;
    ELF::Addr file_end = file_start + phdr->p_filesz;

    ELF::Addr file_page_start = PAGE_START(file_start);
    ELF::Addr file_length = file_end - file_page_start;

    LOG("file_offset=%p file_length=%p start_address=%p end_address=%p",
        file_offset_ + file_page_start, file_length, seg_page_start,
        seg_page_start + PAGE_END(file_length));

    if (file_length != 0) {
      const int prot_flags = PFLAGS_TO_PROT(phdr->p_flags);
      void* seg_addr = fd_.Map((void*)seg_page_start,
                               file_length,
                               prot_flags,
                               MAP_FIXED | MAP_PRIVATE,
                               file_page_start + file_offset_);
      if (!seg_addr) {
        error->Format("Could not map segment %d: %s", i, strerror(errno));
        return false;
      }
    }

    // if the segment is writable, and does not end on a page boundary,
    // zero-fill it until the page limit.
    if ((phdr->p_flags & PF_W) != 0 && PAGE_OFFSET(seg_file_end) > 0) {
      memset((void*)seg_file_end, 0, PAGE_SIZE - PAGE_OFFSET(seg_file_end));
    }

    seg_file_end = PAGE_END(seg_file_end);

    // seg_file_end is now the first page address after the file
    // content. If seg_end is larger, we need to zero anything
    // between them. This is done by using a private anonymous
    // map for all extra pages.
    if (seg_page_end > seg_file_end) {
      void* zeromap = mmap((void*)seg_file_end,
                           seg_page_end - seg_file_end,
                           PFLAGS_TO_PROT(phdr->p_flags),
                           MAP_FIXED | MAP_ANONYMOUS | MAP_PRIVATE,
                           -1,
                           0);
      if (zeromap == MAP_FAILED) {
        error->Format("Could not zero-fill gap: %s", strerror(errno));
        return false;
      }
    }
  }
  return true;
}

}  // namespace

// static
ElfLoader::Result ElfLoader::LoadAt(const char* lib_path,
                                    off_t file_offset,
                                    uintptr_t wanted_address,
                                    Error* error) {
  InternalElfLoader loader;
  Result result;
  if (loader.LoadAt(lib_path, file_offset, wanted_address, error)) {
    result.load_start = reinterpret_cast<ELF::Addr>(loader.load_start());
    result.load_size = loader.load_size();
    result.load_bias = loader.load_bias();
    result.phdr = loader.loaded_phdr();
    result.phdr_count = loader.phdr_count();
    result.reserved_mapping = loader.ReleaseMapping();
  }
  return result;
}

}  // namespace crazy
