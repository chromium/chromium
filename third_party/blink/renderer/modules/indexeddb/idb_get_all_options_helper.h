// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_IDB_GET_ALL_OPTIONS_HELPER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_IDB_GET_ALL_OPTIONS_HELPER_H_

#include <cstdint>

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {
class ExceptionState;
class IDBGetAllOptions;
class ScriptState;

class IDBGetAllOptionsHelper {
  STATIC_ONLY(IDBGetAllOptionsHelper);

 public:
  // Retrieves the arguments to use for a `getAll()` or `getAllKeys()` request.
  // `getAll()` and `getAllKeys()` support a few function overloads, including:
  //
  // 1. Individual optional arguments for query range and count.
  //
  // 2. A single options dictionary argument.  This overload ignores any
  // additional arguments provided like count.  The options dictionary also
  // contains count, which is used instead.
  static IDBGetAllOptions* CreateFromArgumentsOrDictionary(
      ScriptState* script_state,
      const ScriptValue& range_or_options,
      uint32_t max_count,
      ExceptionState& exception_state);

  // Returns the count from the options dictionary when it exists and is
  // non-zero. Otherwise, returns `std::numeric_limits<uint32_t>::max()` as the
  // default value.
  static uint32_t GetCount(const IDBGetAllOptions& options);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_INDEXEDDB_IDB_GET_ALL_OPTIONS_HELPER_H_
