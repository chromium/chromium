// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_REGISTRY_UTIL_WIN_H_
#define REMOTING_BASE_REGISTRY_UTIL_WIN_H_

#include <string>

#include "base/win/windows_types.h"

namespace remoting {

// Writes the default (unnamed) string value to `key_path` under `root_key`
// using the specified `wow64_access` flag (e.g. 0, KEY_WOW64_32KEY,
// KEY_WOW64_64KEY, or `KEY_WOW64_32KEY | KEY_WOW64_64KEY` to write to both
// views). When targeting both views, both writes are attempted even if one
// fails. Creates the key if it does not already exist. Returns true only if
// all attempted writes succeeded.
bool WriteDefaultStringValue(HKEY root_key,
                             const std::wstring& key_path,
                             const std::wstring& value,
                             REGSAM wow64_access = 0);

// Deletes `key_path` under `root_key` using the specified `wow64_access` flag
// (e.g. 0, KEY_WOW64_32KEY, KEY_WOW64_64KEY, or
// `KEY_WOW64_32KEY | KEY_WOW64_64KEY` to delete from both views).
void DeleteKey(HKEY root_key,
               const std::wstring& key_path,
               REGSAM wow64_access = 0);

// Deletes `key_path` under `root_key` only if the key has no subkeys and no
// values, using the specified `wow64_access` flag (e.g. 0, KEY_WOW64_32KEY,
// KEY_WOW64_64KEY, or `KEY_WOW64_32KEY | KEY_WOW64_64KEY` to check and delete
// in both views). Note that this check is not atomic: if another process
// writes a new value between the emptiness check and deletion, that value may
// be deleted. Returns true if the key was deleted, already absent, or
// non-empty (left intact). Returns false if an error occurred while querying
// or deleting the key.
bool DeleteKeyIfEmpty(HKEY root_key,
                      const std::wstring& key_path,
                      REGSAM wow64_access = 0);

}  // namespace remoting

#endif  // REMOTING_BASE_REGISTRY_UTIL_WIN_H_
