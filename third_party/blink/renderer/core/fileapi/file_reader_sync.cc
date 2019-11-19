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
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"

namespace blink {

namespace {
// These values are written to logs.  New enum values can be added, but existing
// enums must never be renumbered or deleted and reused.
enum class WorkerType {
  OTHER = 0,
  DEDICATED_WORKER = 1,
  SHARED_WORKER = 2,
  SERVICE_WORKER = 3,
  MAX
};
}  // namespace

FileReaderSync::FileReaderSync(ExecutionContext* context)
    : task_runner_(context->GetTaskRunner(TaskType::kFileReading)) {
  WorkerType type = WorkerType::OTHER;
  if (context->IsDedicatedWorkerGlobalScope())
    type = WorkerType::DEDICATED_WORKER;
  else if (context->IsSharedWorkerGlobalScope())
    type = WorkerType::SHARED_WORKER;
  else if (context->IsServiceWorkerGlobalScope())
    type = WorkerType::SERVICE_WORKER;
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      EnumerationHistogram, worker_type_histogram,
      ("FileReaderSync.WorkerType", static_cast<int>(WorkerType::MAX)));
  worker_type_histogram.Count(static_cast<int>(type));
}

DOMArrayBuffer* FileReaderSync::readAsArrayBuffer(
    Blob* blob,
    ExceptionState& exception_state) {
  DCHECK(blob);

  std::unique_ptr<FileReaderLoader> loader = std::make_unique<FileReaderLoader>(
      FileReaderLoader::kReadAsArrayBuffer, nullptr, task_runner_);
  StartLoading(*loader, *blob, exception_state);

  return loader->ArrayBufferResult();
}

String FileReaderSync::readAsBinaryString(Blob* blob,
                                          ExceptionState& exception_state) {
  DCHECK(blob);

  std::unique_ptr<FileReaderLoader> loader = std::make_unique<FileReaderLoader>(
      FileReaderLoader::kReadAsBinaryString, nullptr, task_runner_);
  StartLoading(*loader, *blob, exception_state);
  return loader->StringResult();
}

String FileReaderSync::readAsText(Blob* blob,
                                  const String& encoding,
                                  ExceptionState& exception_state) {
  DCHECK(blob);

  std::unique_ptr<FileReaderLoader> loader = std::make_unique<FileReaderLoader>(
      FileReaderLoader::kReadAsText, nullptr, task_runner_);
  loader->SetEncoding(encoding);
  StartLoading(*loader, *blob, exception_state);
  return loader->StringResult();
}

String FileReaderSync::readAsDataURL(Blob* blob,
                                     ExceptionState& exception_state) {
  DCHECK(blob);

  std::unique_ptr<FileReaderLoader> loader = std::make_unique<FileReaderLoader>(
      FileReaderLoader::kReadAsDataURL, nullptr, task_runner_);
  loader->SetDataType(blob->type());
  StartLoading(*loader, *blob, exception_state);
  return loader->StringResult();
}

void FileReaderSync::StartLoading(FileReaderLoader& loader,
                                  const Blob& blob,
                                  ExceptionState& exception_state) {
  loader.Start(blob.GetBlobDataHandle());
  if (loader.GetErrorCode() != FileErrorCode::kOK)
    file_error::ThrowDOMException(exception_state, loader.GetErrorCode());
}

}  // namespace blink
