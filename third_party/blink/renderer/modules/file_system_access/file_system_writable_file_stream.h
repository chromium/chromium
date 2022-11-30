// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_FILE_SYSTEM_ACCESS_FILE_SYSTEM_WRITABLE_FILE_STREAM_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_FILE_SYSTEM_ACCESS_FILE_SYSTEM_WRITABLE_FILE_STREAM_H_

#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_error.mojom-blink.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_file_writer.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_typedefs.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/core/streams/writable_stream_default_writer.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class ScriptPromise;
class ScriptState;
class FileSystemUnderlyingSink;

class FileSystemWritableFileStream final : public WritableStream {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static FileSystemWritableFileStream* Create(
      ScriptState*,
      mojo::PendingRemote<mojom::blink::FileSystemAccessFileWriter>);

  void Trace(Visitor* visitor) const override;

  // IDL defined functions specific to FileSystemWritableFileStream.
  ScriptPromise write(
      ScriptState*,
      const V8UnionBlobOrBufferSourceOrUSVStringOrWriteParams* data,
      ExceptionState&);
  ScriptPromise truncate(ScriptState*, uint64_t size, ExceptionState&);
  ScriptPromise seek(ScriptState*, uint64_t offset, ExceptionState&);

 private:
  Member<FileSystemUnderlyingSink> underlying_sink_;
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_FILE_SYSTEM_ACCESS_FILE_SYSTEM_WRITABLE_FILE_STREAM_H_
