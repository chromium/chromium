// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RLZ_WIN_LIB_REGISTRY_UTIL_H_
#define RLZ_WIN_LIB_REGISTRY_UTIL_H_

#include <stddef.h>

namespace base {
namespace win {
class RegKey;
}  // namespace win
}  // namespace base

namespace rlz_lib {

bool RegKeyReadValue(const base::win::RegKey& key,
                     const wchar_t* name,
                     char* value,
                     size_t* value_size);

bool RegKeyWriteValue(base::win::RegKey* key,
                      const wchar_t* name,
                      const char* value);

bool HasUserKeyAccess(bool write_access);

}  // namespace rlz_lib

#endif  // RLZ_WIN_LIB_REGISTRY_UTIL_H_
