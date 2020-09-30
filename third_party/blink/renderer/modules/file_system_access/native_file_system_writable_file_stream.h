// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_FILE_SYSTEM_ACCESS_NATIVE_FILE_SYSTEM_WRITABLE_FILE_STREAM_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_FILE_SYSTEM_ACCESS_NATIVE_FILE_SYSTEM_WRITABLE_FILE_STREAM_H_

#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/file_system_access/native_file_system_error.mojom-blink.h"
#include "third_party/blink/public/mojom/file_system_access/native_file_system_file_writer.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/array_buffer_or_array_buffer_view_or_blob_or_usv_string_or_write_params.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/core/streams/writable_stream_default_writer.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class ScriptPromise;
class ScriptState;
class NativeFileSystemUnderlyingSink;

class NativeFileSystemWritableFileStream final : public WritableStream {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static NativeFileSystemWritableFileStream* Create(
      ScriptState*,
      mojo::PendingRemote<mojom::blink::NativeFileSystemFileWriter>);

  void Trace(Visitor* visitor) const override;

  // IDL defined functions specific to NativeFileSystemWritableFileStream.
  ScriptPromise write(
      ScriptState*,
      const ArrayBufferOrArrayBufferViewOrBlobOrUSVStringOrWriteParams& data,
      ExceptionState&);
  ScriptPromise truncate(ScriptState*, uint64_t size, ExceptionState&);
  ScriptPromise close(ScriptState*, ExceptionState&);
  ScriptPromise seek(ScriptState*, uint64_t offset, ExceptionState&);

 private:
  Member<NativeFileSystemUnderlyingSink> underlying_sink_;
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_FILE_SYSTEM_ACCESS_NATIVE_FILE_SYSTEM_WRITABLE_FILE_STREAM_H_
