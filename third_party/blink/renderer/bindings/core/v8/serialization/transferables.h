// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SERIALIZATION_TRANSFERABLES_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SERIALIZATION_TRANSFERABLES_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"

namespace blink {

class DOMArrayBufferBase;
class ImageBitmap;
class OffscreenCanvas;
class MessagePort;
class MojoHandle;
class ReadableStream;
class WritableStream;
class TransformStream;

using ArrayBufferArray = HeapVector<Member<DOMArrayBufferBase>>;
using ImageBitmapArray = HeapVector<Member<ImageBitmap>>;
using OffscreenCanvasArray = HeapVector<Member<OffscreenCanvas>>;
using MessagePortArray = HeapVector<Member<MessagePort>>;
using MojoHandleArray = HeapVector<Member<blink::MojoHandle>>;
using ReadableStreamArray = HeapVector<Member<ReadableStream>>;
using WritableStreamArray = HeapVector<Member<WritableStream>>;
using TransformStreamArray = HeapVector<Member<TransformStream>>;

class CORE_EXPORT Transferables final {
  STACK_ALLOCATED();

 public:
  Transferables() = default;
  ~Transferables();

  ArrayBufferArray array_buffers;
  ImageBitmapArray image_bitmaps;
  OffscreenCanvasArray offscreen_canvases;
  MessagePortArray message_ports;
  MojoHandleArray mojo_handles;
  ReadableStreamArray readable_streams;
  WritableStreamArray writable_streams;
  TransformStreamArray transform_streams;

 private:
  DISALLOW_COPY_AND_ASSIGN(Transferables);
};

// Along with extending |Transferables| to hold a new kind of transferable
// objects, serialization handling code changes are required:
//   - extend ScriptValueSerializer::copyTransferables()
//   - alter SerializedScriptValue(Factory) to do the actual transfer.

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SERIALIZATION_TRANSFERABLES_H_
