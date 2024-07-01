// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/shared_dictionary/shared_dictionary_constants.h"

#include "base/functional/callback.h"

namespace network::shared_dictionary {

namespace {

// The size limit per shared dictionary,
constexpr size_t kDictionarySizeLimit = 100 * 1024 * 1024;  // 100 MiB;
size_t g_dictionary_size_limit = kDictionarySizeLimit;

}  // namespace

const char kUseAsDictionaryHeaderName[] = "use-as-dictionary";
const char kOptionNameMatch[] = "match";
const char kOptionNameMatchDest[] = "match-dest";
const char kOptionNameType[] = "type";
const char kOptionNameId[] = "id";

size_t GetDictionarySizeLimit() {
  return g_dictionary_size_limit;
}

base::ScopedClosureRunner SetDictionarySizeLimitForTesting(  // IN-TEST
    size_t dictionary_size_limit) {
  g_dictionary_size_limit = dictionary_size_limit;
  return base::ScopedClosureRunner(
      base::BindOnce([]() { g_dictionary_size_limit = kDictionarySizeLimit; }));
}

}  // namespace network::shared_dictionary
