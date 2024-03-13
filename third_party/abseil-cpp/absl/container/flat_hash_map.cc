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

#include "absl/container/flat_hash_map.h"

#include <cstdint>
#include <string>

#include "absl/base/config.h"

namespace absl {
ABSL_NAMESPACE_BEGIN

ABSL_INTERNAL_TEMPLATE_FLAT_HASH_MAP(template, int32_t, int32_t);
ABSL_INTERNAL_TEMPLATE_FLAT_HASH_MAP(template, std::string, int32_t);
ABSL_INTERNAL_TEMPLATE_FLAT_HASH_MAP(template, int32_t, std::string);
ABSL_INTERNAL_TEMPLATE_FLAT_HASH_MAP(template, int64_t, int64_t);
ABSL_INTERNAL_TEMPLATE_FLAT_HASH_MAP(template, std::string, int64_t);
ABSL_INTERNAL_TEMPLATE_FLAT_HASH_MAP(template, int64_t, std::string);
ABSL_INTERNAL_TEMPLATE_FLAT_HASH_MAP(template, uint32_t, uint32_t);
ABSL_INTERNAL_TEMPLATE_FLAT_HASH_MAP(template, std::string, uint32_t);
ABSL_INTERNAL_TEMPLATE_FLAT_HASH_MAP(template, uint32_t, std::string);
ABSL_INTERNAL_TEMPLATE_FLAT_HASH_MAP(template, uint64_t, uint64_t);
ABSL_INTERNAL_TEMPLATE_FLAT_HASH_MAP(template, std::string, uint64_t);
ABSL_INTERNAL_TEMPLATE_FLAT_HASH_MAP(template, uint64_t, std::string);
ABSL_INTERNAL_TEMPLATE_FLAT_HASH_MAP(template, std::string, std::string);

ABSL_NAMESPACE_END
}  // namespace absl
