// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/indexeddb/idb_get_all_options_helper.h"

#include <limits>

#include "third_party/blink/renderer/bindings/modules/v8/v8_idb_get_all_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_idb_key_range.h"
#include "third_party/blink/renderer/modules/indexeddb/idb_cursor.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {
namespace {

// Attempts to create an options dictionary from `range_or_options`.  Returns
// `nullptr` when `range_or_options` is not a dictionary.
IDBGetAllOptions* TryCreateGetAllOptions(ScriptState* script_state,
                                         v8::Local<v8::Value> range_or_options,
                                         ExceptionState& exception_state) {
  if (!RuntimeEnabledFeatures::IndexedDbGetAllRecordsEnabled()) {
    // `getAllRecords()` introduced the `IDBGetAllOptions` dictionary.
    return nullptr;
  }

  if (range_or_options->IsNullOrUndefined()) {
    return nullptr;
  }

  if (!range_or_options->IsObject()) {
    // `range_or_options` is a simple key value (like number or string).
    return nullptr;
  }

  if (V8IDBKeyRange::HasInstance(script_state->GetIsolate(),
                                 range_or_options)) {
    // `range_or_options` is a key range.
    return nullptr;
  }

  if (range_or_options->IsDate() || range_or_options->IsArray() ||
      range_or_options->IsArrayBuffer() ||
      range_or_options->IsArrayBufferView()) {
    // `range_or_options` is an object key value that is not a dictionary.
    return nullptr;
  }

  // `range_or_options` is an object that can be used to initialize
  // the `IDBGetAllOptions` dictionary.
  return IDBGetAllOptions::Create(script_state->GetIsolate(), range_or_options,
                                  exception_state);
}

}  // namespace

IDBGetAllOptions* IDBGetAllOptionsHelper::CreateFromArgumentsOrDictionary(
    ScriptState* script_state,
    const ScriptValue& range_or_options,
    uint32_t max_count,
    ExceptionState& exception_state) {
  IDBGetAllOptions* options_dictionary = TryCreateGetAllOptions(
      script_state, range_or_options.V8Value(), exception_state);
  if (exception_state.HadException()) {
    return nullptr;
  }

  if (!options_dictionary) {
    // Use the individual arguments for query and count to create
    // `IDBGetAllOptions`.
    options_dictionary = IDBGetAllOptions::Create(script_state->GetIsolate());
    options_dictionary->setQuery(range_or_options);
    options_dictionary->setCount(max_count);
  }
  return options_dictionary;
}

uint32_t IDBGetAllOptionsHelper::GetCount(const IDBGetAllOptions& options) {
  if (options.hasCount() && options.count() > 0) {
    return options.count();
  }
  return std::numeric_limits<uint32_t>::max();
}

}  // namespace blink
