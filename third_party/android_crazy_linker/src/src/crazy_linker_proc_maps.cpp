// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crazy_linker_proc_maps.h"

#include <inttypes.h>
#include <limits.h>

#include "elf_traits.h"
#include "crazy_linker_debug.h"
#include "crazy_linker_line_reader.h"
#include "crazy_linker_util.h"
#include "crazy_linker_system.h"

namespace crazy {

namespace {

// Decompose the components of a /proc/$PID/maps file into multiple
// components. |line| should be the address of a zero-terminated line
// of input. On success, returns true and sets |*entry|, false otherwise.
//
// IMPORTANT: On success, |entry->path| will point into the input line,
// the caller will have to copy the string into a different location if
// it needs to persist it.
bool ParseProcMapsLine(const char* line,
                       const char* line_end,
                       ProcMaps::Entry* entry) {
  // Example input lines on a 64-bit system, one cannot assume that
  // everything is properly sized.
  //
  // 00400000-0040b000 r-xp 00000000 08:01 6570708
  // /bin/cat
  // 0060a000-0060b000 r--p 0000a000 08:01 6570708
  // /bin/cat
  // 0060b000-0060c000 rw-p 0000b000 08:01 6570708
  // /bin/cat
  // 01dd0000-01df1000 rw-p 00000000 00:00 0
  // [heap]
  // 7f4b8d4d7000-7f4b8e22a000 r--p 00000000 08:01 38666648
  // /usr/lib/locale/locale-archive
  // 7f4b8e22a000-7f4b8e3df000 r-xp 00000000 08:01 28836281
  // /lib/x86_64-linux-gnu/libc-2.15.so
  // 7f4b8e3df000-7f4b8e5de000 ---p 001b5000 08:01 28836281
  // /lib/x86_64-linux-gnu/libc-2.15.so
  // 7f4b8e5de000-7f4b8e5e2000 r--p 001b4000 08:01 28836281
  // /lib/x86_64-linux-gnu/libc-2.15.so
  // 7f4b8e5e2000-7f4b8e5e4000 rw-p 001b8000 08:01 28836281
  // /lib/x86_64-linux-gnu/libc-2.15.so
  const char* p = line;
  for (int token = 0; token < 7; ++token) {
    char separator = (token == 0) ? '-' : ' ';
    // skip leading token separators first.
    while (p < line_end && *p == separator)
      p++;

    // find start and end of current token, and compute start of
    // next search. The result of memchr(_,_,0) is undefined, treated as
    // not-found.
    const char* tok_start = p;
    const size_t range = line_end - p;
    const char* tok_end;
    if (range > 0)
      tok_end = static_cast<const char*>(memchr(p, separator, range));
    else
      tok_end = NULL;
    if (!tok_end) {
      tok_end = line_end;
      p = line_end;
    } else {
      p = tok_end + 1;
    }

    if (tok_end == tok_start) {
      if (token == 6) {
        // empty token can happen for index 6, when there is no path
        // element on the line. This corresponds to anonymous memory
        // mapped segments.
        entry->path = NULL;
        entry->path_len = 0;
        break;
      }
      return false;
    }

    switch (token) {
      case 0:  // vma_start
        entry->vma_start = static_cast<size_t>(strtoumax(tok_start, NULL, 16));
        break;

      case 1:  // vma_end
        entry->vma_end = static_cast<size_t>(strtoumax(tok_start, NULL, 16));
        break;

      case 2:  // protection bits
      {
        int flags = 0;
        for (const char* t = tok_start; t < tok_end; ++t) {
          if (*t == 'r')
            flags |= PROT_READ;
          if (*t == 'w')
            flags |= PROT_WRITE;
          if (*t == 'x')
            flags |= PROT_EXEC;
        }
        entry->prot_flags = flags;
      } break;

      case 3:  // page offset
        entry->load_offset =
            static_cast<size_t>(strtoumax(tok_start, NULL, 16)) * PAGE_SIZE;
        break;

      case 6:  // path
        // Get rid of trailing newlines, if any.
        while (tok_end > tok_start && tok_end[-1] == '\n')
          tok_end--;
        entry->path = tok_start;
        entry->path_len = tok_end - tok_start;
        break;

      default:  // ignore all other tokens.
        ;
    }
  }
  return true;
}

}  // namespace

ProcMaps::ProcMaps() {
  static const char kFilePath[] = "/proc/self/maps";
  // NOTE: On Android, /proc/self/maps can easily be more than 10 kiB due
  // to the large number of shared libraries and memory mappings loaded or
  // created by the framework. Use a large capacity to reduce the number
  // of dynamic allocations during parsing.
  const size_t kCapacity = 16000;
  LineReader reader(kFilePath, kCapacity);
  while (reader.GetNextLine()) {
    Entry entry = {};
    if (!ParseProcMapsLine(reader.line(), reader.line() + reader.length(),
                           &entry)) {
      // Ignore broken lines.
      continue;
    }

    // Reallocate path.
    const char* old_path = entry.path;
    if (old_path) {
      char* new_path = static_cast<char*>(::malloc(entry.path_len + 1));
      ::memcpy(new_path, old_path, entry.path_len);
      new_path[entry.path_len] = '\0';
      entry.path = const_cast<const char*>(new_path);
    }

    entries_.PushBack(entry);
  }
}

ProcMaps::~ProcMaps() {
  for (ProcMaps::Entry& entry : entries_) {
    ::free(const_cast<char*>(entry.path));
  }
  entries_.Resize(0);
}

const ProcMaps::Entry* ProcMaps::FindEntryForAddress(void* address) const {
  size_t vma_addr = reinterpret_cast<size_t>(address);
  for (const Entry& entry : entries_) {
    if (entry.vma_start <= vma_addr && vma_addr < entry.vma_end)
      return &entry;
  }
  return nullptr;
}

const ProcMaps::Entry* ProcMaps::FindEntryForFile(const char* file_name) const {
  size_t file_name_len = strlen(file_name);
  bool is_base_name = (strchr(file_name, '/') == NULL);
  for (const Entry& entry : entries_) {
    // Skip vDSO et al.
    if (entry.path_len == 0 || entry.path[0] == '[')
      continue;

    const char* entry_name = entry.path;
    size_t entry_len = entry.path_len;

    if (is_base_name) {
      const char* p = reinterpret_cast<const char*>(
          ::memrchr(entry.path, '/', entry.path_len));
      if (p) {
        entry_name = p + 1;
        entry_len = entry.path_len - (p - entry.path) - 1;
      }
    }

    if (file_name_len == entry_len &&
        !memcmp(file_name, entry_name, entry_len)) {
      return &entry;
    }
  }
  return nullptr;
}

bool FindElfBinaryForAddress(void* address,
                             uintptr_t* load_address,
                             char* path_buffer,
                             size_t path_buffer_len) {
  ProcMaps self_maps;

  uintptr_t addr = reinterpret_cast<uintptr_t>(address);

  const ProcMaps::Entry* entry = self_maps.FindEntryForAddress(address);
  if (!entry) {
    return false;
  }
  *load_address = entry->vma_start;
  if (!entry->path) {
    LOG("Could not find ELF binary path!?");
    return false;
  }
  if (entry->path_len >= path_buffer_len) {
    LOG("ELF binary path too long: '%.*s'", entry->path_len, entry->path);
    return false;
  }
  ::memcpy(path_buffer, entry->path, entry->path_len);
  path_buffer[entry->path_len] = '\0';
  return true;
}

bool FindLoadAddressForFile(const char* file_name,
                            uintptr_t* load_address,
                            uintptr_t* load_offset) {
  ProcMaps self_maps;
  const ProcMaps::Entry* entry = self_maps.FindEntryForFile(file_name);
  if (!entry) {
    return false;
  }
  *load_address = entry->vma_start;
  *load_offset = entry->load_offset;
  return true;
}

}  // namespace crazy
