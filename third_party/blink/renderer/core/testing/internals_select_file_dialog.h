// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_INTERNALS_SELECT_FILE_DIALOG_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_INTERNALS_SELECT_FILE_DIALOG_H_

#include <optional>

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class ExceptionState;
class Internals;
class ScriptState;

class InternalsSelectFileDialog {
  STATIC_ONLY(InternalsSelectFileDialog);

 public:
  static ScriptPromise<IDLUndefined> setSelectFileDialogResult(
      ScriptState* script_state,
      Internals&,
      const std::optional<Vector<String>>& paths,
      ExceptionState& exception_state);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_INTERNALS_SELECT_FILE_DIALOG_H_
