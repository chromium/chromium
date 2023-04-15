// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/pipe_options.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_stream_pipe_options.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"

namespace blink {

PipeOptions::PipeOptions(const StreamPipeOptions* options)
    : prevent_close_(options->hasPreventClose() ? options->preventClose()
                                                : false),
      prevent_abort_(options->hasPreventAbort() ? options->preventAbort()
                                                : false),
      prevent_cancel_(options->hasPreventCancel() ? options->preventCancel()
                                                  : false),
      signal_(options->hasSignal() ? options->signal() : nullptr) {}

void PipeOptions::Trace(Visitor* visitor) const {
  visitor->Trace(signal_);
}

}  // namespace blink
