// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_WRITER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_WRITER_H_

#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/native_file_system/native_file_system_error.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/native_file_system/native_file_system_file_writer.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/array_buffer_or_array_buffer_view_or_blob_or_usv_string.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class Blob;
class ExceptionState;
class FetchDataLoader;
class ReadableStream;
class ScriptPromise;
class ScriptPromiseResolver;
class ScriptState;
class NativeFileSystemFileHandle;

class NativeFileSystemWriter final : public ScriptWrappable,
                                     public ContextLifecycleObserver {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(NativeFileSystemWriter);

 public:
  NativeFileSystemWriter(
      ExecutionContext* context,
      mojo::PendingRemote<mojom::blink::NativeFileSystemFileWriter>);

  ScriptPromise write(ScriptState*,
                      uint64_t position,
                      const ArrayBufferOrArrayBufferViewOrBlobOrUSVString& data,
                      ExceptionState&);
  ScriptPromise truncate(ScriptState*, uint64_t size);
  ScriptPromise close(ScriptState*);

  void Trace(Visitor*) override;
  void ContextDestroyed(ExecutionContext*) override;

 private:
  class StreamWriterClient;

  ScriptPromise WriteBlob(ScriptState*, uint64_t position, Blob*);
  ScriptPromise WriteStream(ScriptState*,
                            uint64_t position,
                            ReadableStream* stream,
                            ExceptionState&);

  void WriteComplete(mojom::blink::NativeFileSystemErrorPtr result,
                     uint64_t bytes_written);
  void TruncateComplete(mojom::blink::NativeFileSystemErrorPtr result);
  void CloseComplete(mojom::blink::NativeFileSystemErrorPtr result);

  mojo::Remote<mojom::blink::NativeFileSystemFileWriter> writer_remote_;
  Member<NativeFileSystemFileHandle> file_;

  Member<ScriptPromiseResolver> pending_operation_;
  Member<FetchDataLoader> stream_loader_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_WRITER_H_
