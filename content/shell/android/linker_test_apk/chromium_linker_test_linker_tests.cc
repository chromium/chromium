// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file implements the native methods of
// org.content.chromium.app.LinkerTests
// Unlike the content of linker_jni.cc, it is part of the content library and
// can thus use base/ and the C++ STL.

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <string>
#include <vector>

#include "base/debug/proc_maps_linux.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "content/shell/android/linker_test_jni_headers/LinkerTests_jni.h"
#include "third_party/re2/src/re2/re2.h"

using base::android::JavaParamRef;

namespace content {

namespace {

using base::debug::MappedMemoryRegion;

jboolean RunChecks(bool in_browser_process) {
  // IMPORTANT NOTE: The Python test control script reads the logcat for
  // lines like:
  //   BROWSER_LINKER_TEST: <status>
  //   RENDERER_LINKER_TEST: <status>
  //
  // Where <status> can be either SUCCESS or FAIL. Other lines starting
  // with the same prefixes, but not using SUCCESS or FAIL are ignored.
  const char* prefix =
      in_browser_process ? "BROWSER_LINKER_TEST: " : "RENDERER_LINKER_TEST: ";

  // The RELRO section(s) will appear in /proc/self/maps as a mapped memory
  // region for a file with a recognizable name. For the LegacyLinker the
  // full name will be something like:
  //
  //   "/dev/ashmem/RELRO:<libname> (deleted)"
  //
  // and for the ModernLinker, something like:
  //
  //   "/data/data/org.chromium.chromium_linker_test_apk/
  //       app_chromium_linker_test/RELRO:<libname> (deleted)"
  //
  // Where <libname> is the library name and '(deleted)' is actually
  // added by the kernel to indicate there is no corresponding file
  // on the filesystem.
  //
  // For regular builds, there is only one library, and thus one RELRO
  // section, but for the component build, there are several libraries,
  // each one with its own RELRO.
  static const char kLegacyRelroSectionPattern[] = "/dev/ashmem/RELRO:.*";
  static const char kModernRelroSectionPattern[] = "/data/.*/RELRO:.*";

  // Parse /proc/self/maps and builds a list of region mappings in this
  // process.
  std::string maps;
  base::debug::ReadProcMaps(&maps);
  if (maps.empty()) {
    LOG(ERROR) << prefix << "FAIL Cannot parse /proc/self/maps";
    return false;
  }

  std::vector<MappedMemoryRegion> regions;
  base::debug::ParseProcMaps(maps, &regions);
  if (regions.empty()) {
    LOG(ERROR) << prefix << "FAIL Cannot read memory mappings in this process";
    return false;
  }

  const RE2 legacy_linker_re(kLegacyRelroSectionPattern);
  const RE2 modern_linker_re(kModernRelroSectionPattern);

  int num_shared_relros = 0;
  int num_bad_shared_relros = 0;

  for (size_t n = 0; n < regions.size(); ++n) {
    MappedMemoryRegion& region = regions[n];

    const std::string path = region.path;
    const bool is_legacy_relro = re2::RE2::FullMatch(path, legacy_linker_re);
    const bool is_modern_relro = re2::RE2::FullMatch(path, modern_linker_re);

    if (is_legacy_relro && is_modern_relro) {
      LOG(ERROR) << prefix
                 << "FAIL RELRO cannot be both Legacy and Modern (test error)";
      return false;
    }

    if (!is_legacy_relro && !is_modern_relro) {
      // Ignore any mapping that isn't a shared RELRO.
      continue;
    }

    num_shared_relros++;

    void* region_start = reinterpret_cast<void*>(region.start);
    void* region_end = reinterpret_cast<void*>(region.end);

    // Check that it is mapped read-only.
    const uint8_t expected_flags = MappedMemoryRegion::READ;
    const uint8_t expected_mask = MappedMemoryRegion::READ |
                                  MappedMemoryRegion::WRITE |
                                  MappedMemoryRegion::EXECUTE;

    uint8_t region_flags = region.permissions & expected_mask;
    if (region_flags != expected_flags) {
      LOG(ERROR)
          << prefix
          << base::StringPrintf(
                 "Shared RELRO section at %p-%p is not mapped read-only. "
                 "Protection flags are %d (%d expected)!",
                 region_start,
                 region_end,
                 region_flags,
                 expected_flags);
      num_bad_shared_relros++;
      continue;
    }

    // Shared RELROs implemented by ModernLinker are not in ashmem. ModernLinker
    // (via android_dlopen_ext()) maps everything with MAP_PRIVATE rather than
    // MAP_SHARED. Remapping such a RELRO section read-write will therefore
    // succeed, but it is not a problem. The memory copy-on-writes, and updates
    // are not visible to either the mapped file or other processes mapping the
    // same file. So... we skip the remap test for ModernLinker.
    if (is_modern_relro) {
      continue;
    }

    // Check that trying to remap it read-write fails with EACCES
    size_t region_size = region.end - region.start;
    int ret = ::mprotect(region_start, region_size, PROT_READ | PROT_WRITE);
    if (ret != -1) {
      LOG(ERROR)
          << prefix
          << base::StringPrintf(
                 "Shared RELRO section at %p-%p could be remapped read-write!?",
                 region_start,
                 region_end);
      num_bad_shared_relros++;
      // Just in case.
      ::mprotect(region_start, region_size, PROT_READ);
    } else if (errno != EACCES) {
      LOG(ERROR) << prefix << base::StringPrintf(
                                  "Shared RELRO section at %p-%p failed "
                                  "read-write mprotect with "
                                  "unexpected error %d (EACCES:%d wanted): %s",
                                  region_start,
                                  region_end,
                                  errno,
                                  EACCES,
                                  strerror(errno));
      num_bad_shared_relros++;
    }
  }

  VLOG(0) << prefix
          << base::StringPrintf(
                 "There are %d shared RELRO sections in this process, of which "
                 "%d are bad",
                 num_shared_relros, num_bad_shared_relros);

  if (num_bad_shared_relros > 0) {
    LOG(ERROR) << prefix << "FAIL Bad RELROs sections in this process";
    return false;
  }

  if (num_shared_relros == 0) {
    LOG(ERROR) << prefix
               << "FAIL Missing shared RELRO sections in this process!";
    return false;
  }

  VLOG(0) << prefix << "SUCCESS";
  return true;
}

}  // namespace

jboolean JNI_LinkerTests_CheckForSharedRelros(JNIEnv* env,
                                              jboolean in_browser_process) {
  return RunChecks(in_browser_process);
}

}  // namespace content
