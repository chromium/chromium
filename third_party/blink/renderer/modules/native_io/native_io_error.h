// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_NATIVE_IO_NATIVE_IO_ERROR_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_NATIVE_IO_NATIVE_IO_ERROR_H_

#include "base/files/file.h"
#include "third_party/blink/public/mojom/native_io/native_io.mojom-blink.h"

using blink::mojom::blink::NativeIOErrorPtr;
using blink::mojom::blink::NativeIOErrorType;

namespace blink {

class ScriptPromiseResolver;
class ExceptionState;

// Reject the `resolver` with the appropriate DOMException given
// `error`. The resolver's execution context should be valid.
void RejectNativeIOWithError(ScriptPromiseResolver* resolver,
                             NativeIOErrorPtr error);

// Reject the `resolver` with the appropriate DOMException given `file_error`.
// When no `message` is provided, the default one is chosen. The resolver's
// execution context should be valid.
void RejectNativeIOWithError(ScriptPromiseResolver* resolver,
                             base::File::Error file_error,
                             const String& message = String());

// Throw with the appropriate DOMException given `error`.
void ThrowNativeIOWithError(ExceptionState& exception_state,
                            NativeIOErrorPtr error);

// Throw with the appropriate DOMException given `file_error`. When no `message`
// is provided, the standard one is chosen.
void ThrowNativeIOWithError(ExceptionState& exception_state,
                            base::File::Error file_error,
                            const String& message = String());

NativeIOErrorPtr FileErrorToNativeIOError(base::File::Error file_error,
                                          const String& message);
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_NATIVE_IO_NATIVE_IO_ERROR_H_
