//
//  Copyright 2019 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "absl/flags/flag.h"

#include <cstring>

namespace absl {

// We want to validate the type mismatch between type definition and
// declaration. The lock-free implementation does not allow us to do it,
// so in debug builds we always use the slower implementation, which always
// validates the type.
#ifndef NDEBUG
#define ABSL_FLAGS_ATOMIC_GET(T) \
  T GetFlag(const absl::Flag<T>& flag) { return flag.Get(); }
#else
#define ABSL_FLAGS_ATOMIC_GET(T)         \
  T GetFlag(const absl::Flag<T>& flag) { \
    T result;                            \
    if (flag.AtomicGet(&result)) {       \
      return result;                     \
    }                                    \
    return flag.Get();                   \
  }
#endif

ABSL_FLAGS_INTERNAL_FOR_EACH_LOCK_FREE(ABSL_FLAGS_ATOMIC_GET)

#undef ABSL_FLAGS_ATOMIC_GET

}  // namespace absl
