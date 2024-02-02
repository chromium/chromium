/*
 * Copyright (C) 2010 Google Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/fileapi/file_reader_sync.h"

#include <memory>

#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/core/fileapi/file_error.h"
#include "third_party/blink/renderer/core/fileapi/file_reader_loader.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

FileReaderSync::FileReaderSync(ExecutionContext* context)
    : task_runner_(context->GetTaskRunner(TaskType::kFileReading)) {}

DOMArrayBuffer* FileReaderSync::readAsArrayBuffer(
    Blob* blob,
    ExceptionState& exception_state) {
  DCHECK(blob);

  std::optional<FileReaderData> res = Load(*blob, exception_state);
  return !res ? nullptr : std::move(res).value().AsDOMArrayBuffer();
}

String FileReaderSync::readAsBinaryString(Blob* blob,
                                          ExceptionState& exception_state) {
  DCHECK(blob);

  std::optional<FileReaderData> res = Load(*blob, exception_state);
  if (!res) {
    return "";
  }
  return std::move(res).value().AsBinaryString();
}

String FileReaderSync::readAsText(Blob* blob,
                                  const String& encoding,
                                  ExceptionState& exception_state) {
  DCHECK(blob);

  std::optional<FileReaderData> res = Load(*blob, exception_state);
  if (!res) {
    return "";
  }
  return std::move(res).value().AsText(encoding);
}

String FileReaderSync::readAsDataURL(Blob* blob,
                                     ExceptionState& exception_state) {
  DCHECK(blob);

  std::optional<FileReaderData> res = Load(*blob, exception_state);
  if (!res) {
    return "";
  }
  return std::move(res).value().AsDataURL(blob->type());
}

std::optional<FileReaderData> FileReaderSync::Load(
    const Blob& blob,
    ExceptionState& exception_state) {
  auto res =
      SyncedFileReaderAccumulator::Load(blob.GetBlobDataHandle(), task_runner_);
  if (res.first != FileErrorCode::kOK) {
    file_error::ThrowDOMException(exception_state, res.first);
    return std::nullopt;
  }
  return std::move(res.second);
}

}  // namespace blink
