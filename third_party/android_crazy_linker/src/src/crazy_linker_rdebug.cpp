// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crazy_linker_rdebug.h"

#include <elf.h>
#include <inttypes.h>
#include <limits.h>
#include <sys/auxv.h>
#include <sys/mman.h>
#include <unistd.h>

#include "crazy_linker_debug.h"
#include "crazy_linker_globals.h"
#include "crazy_linker_system.h"
#include "crazy_linker_util.h"
#include "elf_traits.h"

namespace crazy {

namespace {

// The <sys/auxv.h> header only declares getauxval for API level >= 18.
// Declare the same function as a weak import here so we can detect at
// runtime that getauxval() is not provided, i.e. this is running on an
// older Android release.
extern "C" unsigned long getauxval(unsigned long) __attribute__((weak));

// Retrieve the address of the current process' dynamic section.
bool FindElfDynamicSection(size_t* dynamic_address, size_t* dynamic_size) {
  // Sanity check. Prevents crashing when running on an Android release older
  // than KitKat / API 18. The linker will still work, but debugging and
  // stack crashes will not be supported.
  if (!getauxval) {
    LOG("Android API level 18 or higher is needed for stack trace support!");
    return false;
  }

  // Use getauxval() to get the address and size of the executable's
  // program table entry. Note: On Android, getauxval() is only available
  // starting with API level 18.
  const size_t phdr_num = static_cast<size_t>(getauxval(AT_PHNUM));
  const auto* phdr_table = reinterpret_cast<ELF::Phdr*>(getauxval(AT_PHDR));
  LOG("Found phdr table at %p, count=%d", phdr_table, phdr_num);
  if (!phdr_table) {
    LOG_ERRNO("Could not retrieve program header with AT_PHDR");
    return false;
  }

  // NOTE: The program header table contains the following interesting entries:
  // - A PT_PHDR entry corresponding to the program header table itself!
  // - A PT_DYNAMIC entry corresponding to the dynamic section.
  const ELF::Phdr* pt_phdr = nullptr;
  const ELF::Phdr* pt_dynamic = nullptr;
  for (size_t n = 0; n < phdr_num; ++n) {
    const ELF::Phdr* phdr = &phdr_table[n];
    if (phdr->p_type == PT_PHDR && !pt_phdr)
      pt_phdr = phdr;
    else if (phdr->p_type == PT_DYNAMIC && !pt_dynamic)
      pt_dynamic = phdr;
  }

  if (!pt_phdr) {
    LOG("Could not find PT_PHDR entry!?");
    return false;
  }
  if (!pt_dynamic) {
    LOG("Could not find PT_DYNAMIC entry!?");
    return false;
  }

  LOG("Found PT_PHDR [address=%p vaddr=%lu size=%lu] and "
      "PT_DYNAMIC [vaddr=%lu, size=%lu]",
      pt_phdr, static_cast<unsigned long>(pt_phdr->p_vaddr),
      static_cast<unsigned long>(pt_phdr->p_memsz),
      static_cast<unsigned long>(pt_dynamic->p_vaddr),
      static_cast<unsigned long>(pt_dynamic->p_memsz));

  auto pt_hdr_address = reinterpret_cast<ptrdiff_t>(pt_phdr);
  auto load_bias = pt_hdr_address - static_cast<ptrdiff_t>(pt_phdr->p_vaddr);

  *dynamic_address = static_cast<size_t>(load_bias + pt_dynamic->p_vaddr);
  *dynamic_size = static_cast<size_t>(pt_dynamic->p_memsz);

  LOG("Dynamic section addr=%p size=%p", (void*)*dynamic_address,
      (void*)*dynamic_size);
  return true;
}

// Helper function for AddEntryImpl and DelEntryImpl.
// Sets *link_pointer to entry.  link_pointer is either an 'l_prev' or an
// 'l_next' field in a neighbouring linkmap_t.  If link_pointer is in a
// page that is mapped readonly, the page is remapped to be writable before
// assignment.
void WriteLinkMapField(link_map_t** link_pointer, link_map_t* entry) {
  // We always mprotect the page containing link_pointer to read/write,
  // then write the entry. The page may already be read/write, but on
  // recent Android release is most likely readonly. Because of the way
  // the system linker operates we cannot tell with certainty what its
  // correct setting should be.
  //
  // Now, we always leave the page read/write. Here is why. If we set it
  // back to readonly at the point between where the system linker sets
  // it to read/write and where it writes to the address, this will cause
  // the system linker to crash. Clearly that is undesirable. From
  // observations this occurs most frequently on the gpu process.
  //
  // https://code.google.com/p/chromium/issues/detail?id=450659
  // https://code.google.com/p/chromium/issues/detail?id=458346
  const uintptr_t kPageSize = PAGE_SIZE;
  const uintptr_t ptr_address = reinterpret_cast<uintptr_t>(link_pointer);
  void* page = reinterpret_cast<void*>(ptr_address & ~(kPageSize - 1U));

  LOG("Mapping page at %p read-write for pointer at %p", page, link_pointer);
  const int prot = PROT_READ | PROT_WRITE;
  const int ret = ::mprotect(page, kPageSize, prot);
  if (ret < 0) {
    // In case of error, return immediately to avoid crashing below when
    // writing the new value. Note that there is still a tiny chance that the
    // system linker remapped the page read-only just after mprotect() above
    // returns, so this cannot be guaranteed 100% of the time.
    LOG_ERRNO("Error mapping page %p read/write", page);
    return;
  }
  *link_pointer = entry;
}

}  // namespace

r_debug* RDebug::GetAddress() {
  if (!init_) {
    Init();
  }
  return r_debug_;
}

bool RDebug::Init() {
  // The address of '_r_debug' is in the DT_DEBUG entry of the current
  // executable.
  init_ = true;

  size_t dynamic_addr = 0;
  size_t dynamic_size = 0;

  if (!FindElfDynamicSection(&dynamic_addr, &dynamic_size)) {
    return false;
  }

  // Parse the dynamic table and find the DT_DEBUG entry.
  const ELF::Dyn* dyn_section = reinterpret_cast<const ELF::Dyn*>(dynamic_addr);

  while (dynamic_size >= sizeof(*dyn_section)) {
    if (dyn_section->d_tag == DT_DEBUG) {
      // Found it!
      LOG("Found DT_DEBUG entry at %p, pointing to %p", dyn_section,
          dyn_section->d_un.d_ptr);
      if (dyn_section->d_un.d_ptr) {
        r_debug_ = reinterpret_cast<r_debug*>(dyn_section->d_un.d_ptr);
        LOG("r_debug [r_version=%d r_map=%p r_brk=%p r_ldbase=%p]",
            r_debug_->r_version, r_debug_->r_map, r_debug_->r_brk,
            r_debug_->r_ldbase);
        // Only version 1 of the struct is supported.
        if (r_debug_->r_version != 1) {
          LOG("r_debug.r_version is %d, 1 expected.", r_debug_->r_version);
          r_debug_ = NULL;
        }
        return true;
      }
    }
    dyn_section++;
    dynamic_size -= sizeof(*dyn_section);
  }

  LOG("There is no non-0 DT_DEBUG entry in this process");
  return false;
}

void RDebug::CallRBrk(int state) {
#if !defined(CRAZY_DISABLE_R_BRK)
  r_debug_->r_state = state;
  r_debug_->r_brk();
#endif  // !CRAZY_DISABLE_R_BRK
}

void RDebug::AddEntry(link_map_t* entry) {
  LOG("Adding: %s", entry->l_name);
  if (!init_)
    Init();

  if (!r_debug_) {
    LOG("Nothing to do");
    return;
  }

  // Ensure modifications to the global link map are synchronized.
  ScopedLinkMapLocker locker;

  // IMPORTANT: GDB expects the first entry in the list to correspond
  // to the executable. So add our new entry just after it. This is ok
  // because by default, the linker is always the second entry, as in:
  //
  //   [<executable>, /system/bin/linker, libc.so, libm.so, ...]
  //
  // By design, the first two entries should never be removed since they
  // can't be unloaded from the process (they are loaded by the kernel
  // when invoking the program).
  //
  // TODO(digit): Does GDB expect the linker to be the second entry?
  // It doesn't seem so, but have a look at the GDB sources to confirm
  // this. No problem appear experimentally.
  //
  // What happens for static binaries? They don't have an .interp section,
  // and don't have a r_debug variable on Android, so GDB should not be
  // able to debug shared libraries at all for them (assuming one
  // statically links a linker into the executable).
  if (!r_debug_->r_map || !r_debug_->r_map->l_next ||
      !r_debug_->r_map->l_next->l_next) {
    // Sanity check: Must have at least two items in the list.
    LOG("Malformed r_debug.r_map list");
    r_debug_ = NULL;
    return;
  }

  // Tell GDB the list is going to be modified.
  CallRBrk(RT_ADD);

  link_map_t* before = r_debug_->r_map->l_next;
  link_map_t* after = before->l_next;

  // Prepare the new entry.
  entry->l_prev = before;
  entry->l_next = after;

  // IMPORTANT: Before modifying the previous and next entries in the
  // list, ensure that they are writable. This avoids crashing when
  // updating the 'l_prev' or 'l_next' fields of a system linker entry,
  // which are mapped read-only.
  WriteLinkMapField(&before->l_next, entry);
  WriteLinkMapField(&after->l_prev, entry);

  // Tell GDB the list modification has completed.
  CallRBrk(RT_CONSISTENT);
}

void RDebug::DelEntry(link_map_t* entry) {
  if (!r_debug_)
    return;

  LOG("Deleting: %s", entry->l_name);

  // Ensure modifications to the global link map are synchronized.
  ScopedLinkMapLocker locker;

  // Tell GDB the list is going to be modified.
  CallRBrk(RT_DELETE);

  // IMPORTANT: Before modifying the previous and next entries in the
  // list, ensure that they are writable. See comment above for more
  // details.
  if (entry->l_prev)
    WriteLinkMapField(&entry->l_prev->l_next, entry->l_next);
  if (entry->l_next)
    WriteLinkMapField(&entry->l_next->l_prev, entry->l_prev);

  if (r_debug_->r_map == entry)
    r_debug_->r_map = entry->l_next;

  entry->l_prev = NULL;
  entry->l_next = NULL;

  // Tell GDB the list modification has completed.
  CallRBrk(RT_CONSISTENT);
}

}  // namespace crazy
