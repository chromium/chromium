// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_CLASSIC_EVALUATION_RESULT_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_CLASSIC_EVALUATION_RESULT_H_

#include "third_party/blink/renderer/bindings/core/v8/script_source_location_type.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_code_cache.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_v8_reference.h"
#include "third_party/blink/renderer/platform/loader/fetch/cached_metadata_handler.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/text_position.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "v8/include/v8.h"

namespace blink {

// ClassicEvaluationResult encapsulates the result of a classic script
// evaluation.
// - If IsEmpty() is false:
//     A script is evaluated successfully.
//     No exceptions are thrown.
//     GetValue() returns a non-Empty value.
// - If IsEmpty() is true:
//     Script evaluation failed (compile error, evaluation error, or so).
//     An exception might or might not be thrown in V8.
//     Unlike v8::MaybeLocal<>, there are cases where no exceptions are thrown
//     to V8 while returning an Empty ClassicEvaluationResult, like when:
//     - An exception is thrown during script evaluation but caught and passed
//       to https://html.spec.whatwg.org/C/#report-the-error, instead of
//       being rethrown, or
//     - Script evaluation is skipped due to checks within Blink.
//
// TODO(crbug/1111134): Consider merging with ModuleEvaluationResult later.
// Right now classic and module evaluation paths are not yet merged, and
// top-level await (crbug/1022182) will modify ModuleEvaluationResult.
class CORE_EXPORT ClassicEvaluationResult final {
  STACK_ALLOCATED();

 public:
  ClassicEvaluationResult() = default;
  explicit ClassicEvaluationResult(v8::Local<v8::Value> value) : value_(value) {
    DCHECK(!IsEmpty());
  }

  bool IsEmpty() const { return value_.IsEmpty(); }
  v8::Local<v8::Value> GetValue() const {
    DCHECK(!IsEmpty());
    return value_;
  }

 private:
  v8::Local<v8::Value> value_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_CLASSIC_EVALUATION_RESULT_H_
