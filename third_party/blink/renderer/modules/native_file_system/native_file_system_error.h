// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_ERROR_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_ERROR_H_

#include "base/files/file.h"
#include "third_party/blink/public/mojom/native_file_system/native_file_system_error.mojom-blink-forward.h"

namespace blink {
class ScriptPromiseResolver;
namespace native_file_system_error {

// Rejects |resolver| with an appropriate exception if |status| represents an
// error. Resolves |resolver| with undefined otherwise.
void ResolveOrReject(ScriptPromiseResolver* resolver,
                     const mojom::blink::NativeFileSystemError& status);

// Rejects |resolver| with an appropriate exception if |status| represents an
// error. DCHECKs otherwise.
void Reject(ScriptPromiseResolver* resolver,
            const mojom::blink::NativeFileSystemError& error);

}  // namespace native_file_system_error
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_ERROR_H_
