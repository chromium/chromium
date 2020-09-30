// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_FILE_SYSTEM_ACCESS_NATIVE_FILE_SYSTEM_UNDERLYING_SINK_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_FILE_SYSTEM_ACCESS_NATIVE_FILE_SYSTEM_UNDERLYING_SINK_H_

#include "third_party/blink/public/mojom/file_system_access/native_file_system_file_writer.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/array_buffer_or_array_buffer_view_or_blob_or_usv_string.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/streams/underlying_sink_base.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

namespace blink {

class ExceptionState;
class ScriptPromiseResolver;
class WriteParams;

class NativeFileSystemUnderlyingSink final : public UnderlyingSinkBase {
 public:
  explicit NativeFileSystemUnderlyingSink(
      ExecutionContext*,
      mojo::PendingRemote<mojom::blink::NativeFileSystemFileWriter>);

  // UnderlyingSinkBase
  ScriptPromise start(ScriptState*,
                      WritableStreamDefaultController*,
                      ExceptionState&) override;
  ScriptPromise write(ScriptState*,
                      ScriptValue chunk,
                      WritableStreamDefaultController*,
                      ExceptionState&) override;
  ScriptPromise close(ScriptState*, ExceptionState&) override;
  ScriptPromise abort(ScriptState*,
                      ScriptValue reason,
                      ExceptionState&) override;

  void Trace(Visitor*) const override;

 private:
  ScriptPromise HandleParams(ScriptState*, const WriteParams&, ExceptionState&);
  ScriptPromise WriteData(
      ScriptState*,
      uint64_t position,
      const ArrayBufferOrArrayBufferViewOrBlobOrUSVString& data,
      ExceptionState&);
  ScriptPromise WriteBlob(ScriptState*,
                          uint64_t position,
                          Blob*,
                          ExceptionState&);
  ScriptPromise Truncate(ScriptState*, uint64_t size, ExceptionState&);
  ScriptPromise Seek(ScriptState*, uint64_t offset, ExceptionState&);
  void WriteComplete(mojom::blink::NativeFileSystemErrorPtr result,
                     uint64_t bytes_written);
  void TruncateComplete(uint64_t to_size,
                        mojom::blink::NativeFileSystemErrorPtr result);
  void CloseComplete(mojom::blink::NativeFileSystemErrorPtr result);

  HeapMojoRemote<mojom::blink::NativeFileSystemFileWriter> writer_remote_;

  uint64_t offset_ = 0;
  Member<ScriptPromiseResolver> pending_operation_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_FILE_SYSTEM_ACCESS_NATIVE_FILE_SYSTEM_UNDERLYING_SINK_H_
