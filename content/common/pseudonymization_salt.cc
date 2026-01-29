// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/pseudonymization_salt.h"

#include <atomic>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/dcheck_is_on.h"
#include "base/logging.h"
#include "base/memory/shared_memory_switch.h"
#include "base/no_destructor.h"
#include "build/blink_buildflags.h"
#include "content/public/common/content_switches.h"

namespace content {

namespace {

std::atomic<uint32_t> g_salt(0);

}  // namespace

uint32_t GetPseudonymizationSalt() {
  uint32_t salt = g_salt.load();

  DCHECK(salt);

  return salt;
}

void SetPseudonymizationSalt(uint32_t salt) {
  DCHECK_NE(0u, salt);

#if DCHECK_IS_ON()
  uint32_t old_salt = g_salt.load(std::memory_order_acquire);
  // Permit the same salt to be set more than once. This is because for single
  // process tests and certain specific tests (e.g.
  // RenderThreadImplBrowserTest), the ChildProcess is running in the same
  // memory space as the browser.
  DCHECK(0 == old_salt || old_salt == salt);
#endif  // DCHECK_IS_ON()

  g_salt.store(salt);
}

void ResetSaltForTesting() {
  g_salt.store(0);
}

bool IsSaltInitialized() {
  return g_salt.load(std::memory_order_acquire) != 0;
}

#if BUILDFLAG(USE_BLINK)
namespace {

base::ReadOnlySharedMemoryRegion CreateSaltSharedMemoryRegion() {
  // Salt must be initialized before creating the shared memory region.
  // This is called when launching child processes, by which point the browser
  // has already initialized the salt during startup.
  CHECK(IsSaltInitialized());
  uint32_t salt = GetPseudonymizationSalt();

  base::MappedReadOnlyRegion mapped_region =
      base::ReadOnlySharedMemoryRegion::Create(sizeof(uint32_t));
  // Creating a 4-byte shared memory region should always succeed.
  CHECK(mapped_region.IsValid());

  uint32_t* salt_ptr = mapped_region.mapping.GetMemoryAs<uint32_t>();
  CHECK(salt_ptr);
  *salt_ptr = salt;

  return std::move(mapped_region.region);
}

}  // namespace

const base::ReadOnlySharedMemoryRegion&
GetPseudonymizationSaltSharedMemoryRegion() {
  static base::NoDestructor<base::ReadOnlySharedMemoryRegion> salt_region(
      CreateSaltSharedMemoryRegion());
  return *salt_region;
}

namespace internal {

bool InitializeSaltFromSharedMemory(base::ReadOnlySharedMemoryRegion region) {
  if (!region.IsValid()) {
    return false;
  }

  base::ReadOnlySharedMemoryMapping mapping = region.Map();
  if (!mapping.IsValid()) {
    return false;
  }

  if (mapping.size() < sizeof(uint32_t)) {
    return false;
  }

  const uint32_t* salt_ptr = mapping.GetMemoryAs<uint32_t>();
  if (!salt_ptr) {
    return false;
  }
  uint32_t salt = *salt_ptr;

  if (salt == 0) {
    return false;
  }

  SetPseudonymizationSalt(salt);
  return true;
}

}  // namespace internal

void MaybeInitializePseudonymizationSaltFromSharedMemory(
    const base::CommandLine& command_line) {
  if (!command_line.HasSwitch(switches::kPseudonymizationSaltHandle)) {
    return;
  }

  auto region_result = base::shared_memory::ReadOnlySharedMemoryRegionFrom(
      command_line.GetSwitchValueASCII(switches::kPseudonymizationSaltHandle));
  if (region_result.has_value()) {
    internal::InitializeSaltFromSharedMemory(std::move(region_result.value()));
  } else {
    DLOG(WARNING) << "Failed to read pseudonymization salt from shared memory";
  }
}
#endif  // BUILDFLAG(USE_BLINK)

}  // namespace content
