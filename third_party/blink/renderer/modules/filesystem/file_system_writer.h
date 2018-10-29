// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_FILESYSTEM_FILE_SYSTEM_WRITER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_FILESYSTEM_FILE_SYSTEM_WRITER_H_

#include "third_party/blink/public/mojom/filesystem/file_writer.mojom-blink.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_member.h"

namespace blink {

class Blob;
class ExceptionState;
class FetchDataLoader;
class ScriptPromise;
class ScriptPromiseResolver;
class ScriptState;
class ScriptValue;

class FileSystemWriter final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit FileSystemWriter(mojom::blink::FileWriterPtr);

  ScriptPromise write(ScriptState*,
                      uint64_t position,
                      ScriptValue data,
                      ExceptionState&);
  ScriptPromise truncate(ScriptState*, uint64_t size);
  ScriptPromise close(ScriptState*);

  void Trace(Visitor*) override;

 private:
  class StreamWriterClient;

  ScriptPromise WriteBlob(ScriptState*, uint64_t position, Blob*);
  ScriptPromise WriteStream(ScriptState*,
                            uint64_t position,
                            ScriptValue stream,
                            ExceptionState&);

  void WriteComplete(base::File::Error result, uint64_t bytes_written);
  void TruncateComplete(base::File::Error result);

  mojom::blink::FileWriterPtr writer_;

  Member<ScriptPromiseResolver> pending_operation_;
  TraceWrapperMember<FetchDataLoader> stream_loader_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_FILESYSTEM_FILE_SYSTEM_WRITER_H_
