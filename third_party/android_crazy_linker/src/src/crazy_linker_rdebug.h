// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRAZY_LINKER_RDEBUG_H
#define CRAZY_LINKER_RDEBUG_H

#include <stdint.h>

// The system linker maintains two lists of libraries at runtime:
//
// - A list that is used by GDB and other tools to search for the
//   binaries that are loaded in the process.
//
//   This list is accessible by looking at the DT_DEBUG field of the
//   dynamic section of any ELF binary loaded by the linker (including
//   itself). The field contains the address of a global '_r_debug'
//   variable. More on this later.
//
// - A list that is used internally to implement library and symbol
//   lookup. The list head and tail are called 'solist' and 'sonext'
//   in the linker sources, and their address is unknown (and randomized
//   by ASLR), and there is no point trying to change it.
//
// This means that you cannot call the linker's dlsym() function to
// lookup symbols in libraries that are not loaded through it, i.e.
// any custom dynamic linker needs its own dlopen() / dlsym() and other
// related functions, and ensure the loaded code only uses its own version.
// (See support code in crazy_linker_wrappers.cpp)
//
// The global '_r_debug' variable is a r_debug structure, whose layout
// must be known by GDB, with the following fields:
//
//     r_version   -> version of the structure (must be 1)
//     r_map       -> head of a linked list of 'link_map_t' entries,
//                    one per ELF 'binary' in the process address space.
//     r_brk       -> pointer to a specific debugging function (see below).
//     r_state     -> state variable to be read in r_brk().
//     r_ldbase    -> unused by the system linker, should be 0. (?)
//
// The 'r_brk' field points to an empty function in the system linker
// that is used to notify GDB of changes in the list of shared libraries,
// this works as follows:
//
//   - When the linker wants to add a new shared library to the list,
//     it first writes RT_ADD to 'r_state', then calls 'r_brk()'.
//
//     It modifies the list, then writes RT_CONSISTENT to 'r_state' and
//     calls 'r_brk()' again.
//
//   - When unloading a library, RT_DELETE + RT_CONSISTENT are used
//     instead.
//
// GDB will always place a breakpoint on the function pointed to by
// 'r_brk', and will be able to synchronize with the linker's
// modifications.
//
// The 'r_map' field is a list of nodes with the following structure
// describing each loaded shared library for GDB:
//
//   l_addr  -> Load address of the first PT_LOAD segment in a
//              shared library. Note that this is 0 for the linker itself
//              and the load-bias for an executable.
//   l_name  -> Name of the executable. This is _always_ a basename!!
//   l_ld    -> Address of the dynamic table for this binary.
//   l_next  -> Pointer to next item in 'r_map' list or NULL.
//   l_prev  -> Pointer to previous item in 'r_map' list.
//
// Note that the system linker ensures that there are always at least
// two items in this list:
//
// - The first item always describes the linker itself, the fields
//   actually point to a specially crafted fake entry for it called
//   'libdl_info' in the linker sources.
//
// - The second item describes the executable that was started by
//   the kernel. For Android applications, it will always be 'app_process'
//   and completely uninteresting.
//
// - Eventually, another entry for VDSOs on platforms that support them.
//
// When implementing a custom linker, being able to debug the process
// unfortunately requires modifying the 'r_map' list to also account
// for libraries loading through it.
//
// One issue with this is that the linker also uses another internal
// variable, called '_r_debut_tail' that points to the last item in
// the list. And there is no way to access it directly. This can lead
// to problems when calling APIs that actually end up using the system's
// own dlopen(). Consider this example:
//
//  1/ Program loads crazy_linker
//
//  2/ Program uses crazy_linker to load libfoo.so, this adds
//     a new entry at the end of the '_r_debug.r_map' list, but
//     '_r_debug.tail' is unmodified.
//
//  3/ libfoo.so or the Java portion of the program calls a system API
//     that ends up loading another library (e.g. libGLESv2_vendor.so),
//     this calls the system dlopen().
//
//  4/ The system dlopen() adds a new entry to the "_r_debug.r_map"
//     list by updating the l_next / l_prev fields of the entry pointed
//     to by '_r_debug_tail', and this removes 'libfoo.so' from the list!
//
// There is a simple work-around for this issue: Always insert our
// libraries at the _start_ of the 'r_map' list, instead of appending
// them to the end. The system linker doesn't know about custom-loaded
// libraries and thus will never try to unload them.
//
// Note that the linker never uses the 'r_map' list (except or updating
// it for GDB), it only uses 'solist / sonext' to actually perform its
// operations. That's ok if our custom linker completely wraps and
// re-implements these.
//
// The system linker expects to be the only item modifying the 'r_map'
// list, and as such it may set the pages that contain the list readonly
// outside of its own modifications. In threaded environments where the
// system linker and the crazy linker are operating simultaneously on
// different threads this may be a problem; we need these pages to be
// writable when we have to update the list.
//
// TECHNICAL NOTE: If CRAZY_DISABLE_R_BRK is defined at compile time,
// then the crazy linker will never try to call the r_brk() GDB Hook
// function. This can be useful to avoid runtime crashes on certain
// Android devices with x86 processors, running ARM binaries with
// a machine translator like Houdini. See http://crbug.com/796938
//
namespace crazy {

struct link_map_t {
  uintptr_t l_addr;
  char* l_name;
  uintptr_t l_ld;
  link_map_t* l_next;
  link_map_t* l_prev;
};

// Values for r_debug->r_state
enum {
  RT_CONSISTENT,
  RT_ADD,
  RT_DELETE
};

struct r_debug {
  int32_t r_version;
  link_map_t* r_map;
  void (*r_brk)(void);
  int32_t r_state;
  uintptr_t r_ldbase;
};

class RDebug {
 public:
  RDebug() = default;
  ~RDebug() = default;

  RDebug(const RDebug&) = delete;
  RDebug& operator=(const RDebug&) = delete;

  // Add entries to and remove entries from the list.
  void AddEntry(link_map_t* entry);
  void DelEntry(link_map_t* entry);

  // Return address of current global _r_debug variable, or nullptr if not
  // available.
  r_debug* GetAddress();

 private:
  // Try to find the address of the global _r_debug variable, even
  // though there is no symbol for it. Returns true on success.
  bool Init();

  // Call the debugger hook function |r_debug_->r_brk|.
  // |state| is the value to write to |r_debug_->r_state|
  // before that. This is done to coordinate with the
  // debugger when modifications of the global |r_debug_|
  // list are performed.
  void CallRBrk(int state);

  r_debug* r_debug_ = nullptr;
  bool init_ = false;
};

}  // namespace crazy

#endif  // CRAZY_LINKER_REDUG_H
