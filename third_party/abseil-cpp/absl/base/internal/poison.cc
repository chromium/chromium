// Copyright 2024 The Abseil Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "absl/base/internal/poison.h"

#include <atomic>
#include <cstdint>  // NOLINT - used in ifdef
#include <cstdlib>

#include "absl/base/attributes.h"
#include "absl/base/config.h"

#if defined(ABSL_HAVE_ADDRESS_SANITIZER)
#include <sanitizer/asan_interface.h>
#elif defined(ABSL_HAVE_MEMORY_SANITIZER)
#include <sanitizer/msan_interface.h>
#elif defined(ABSL_HAVE_MMAP)
#include <sys/mman.h>
#elif defined(_MSC_VER)
#include <windows.h>
#endif

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace base_internal {
namespace {
constexpr size_t kPageSize = 1 << 12;
alignas(kPageSize) static char poison_page[kPageSize];
}  // namespace

std::atomic<void*> poison_data = {&poison_page};

namespace {

#if defined(ABSL_HAVE_ADDRESS_SANITIZER)
void PoisonBlock(void* data) { ASAN_POISON_MEMORY_REGION(data, kPageSize); }
#elif defined(ABSL_HAVE_MEMORY_SANITIZER)
void PoisonBlock(void* data) { __msan_poison(data, kPageSize); }
#elif defined(ABSL_HAVE_MMAP)
void PoisonBlock(void* data) { mprotect(data, kPageSize, PROT_NONE); }
#elif defined(_MSC_VER)
void PoisonBlock(void* data) {
  DWORD old_mode = 0;
  VirtualProtect(data, kPageSize, PAGE_NOACCESS, &old_mode);
}
#else
void PoisonBlock(void* data) {
  // We can't make poisoned memory, so just use a likely bad pointer.
  // Pointers are required to have high bits that are all zero or all one for
  // certain 64-bit CPUs. This pointer value will hopefully cause a crash on
  // dereference and also be clearly recognizable as invalid.
  constexpr uint64_t kBadPtr = 0xBAD0BAD0BAD0BAD0;
  poison_data = reinterpret_cast<void*>(static_cast<uintptr_t>(kBadPtr));
}
#endif

void* InitializePoisonedPointer() {
  PoisonBlock(&poison_page);
  return &poison_page;
}

}  // namespace

ABSL_ATTRIBUTE_UNUSED void* force_initialize = InitializePoisonedPointer();

}  // namespace base_internal
ABSL_NAMESPACE_END
}  // namespace absl
