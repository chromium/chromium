// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_MOJO_MOJO_HANDLE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_MOJO_MOJO_HANDLE_H_

#include "mojo/public/cpp/system/core.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class ArrayBufferOrArrayBufferView;
class MojoCreateSharedBufferResult;
class MojoDiscardDataOptions;
class MojoDuplicateBufferHandleOptions;
class MojoHandleSignals;
class MojoMapBufferResult;
class MojoReadDataOptions;
class MojoReadDataResult;
class MojoReadMessageFlags;
class MojoReadMessageResult;
class MojoWatcher;
class MojoWriteDataOptions;
class MojoWriteDataResult;
class ScriptState;
class V8MojoWatchCallback;

class CORE_EXPORT MojoHandle final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit MojoHandle(mojo::ScopedHandle);

  mojo::ScopedHandle TakeHandle();

  void close();
  MojoWatcher* watch(ScriptState*,
                     const MojoHandleSignals*,
                     V8MojoWatchCallback*);

  // MessagePipe handle.
  MojoResult writeMessage(ArrayBufferOrArrayBufferView&,
                          const HeapVector<Member<MojoHandle>>&);
  MojoReadMessageResult* readMessage(const MojoReadMessageFlags*);

  // DataPipe handle.
  MojoWriteDataResult* writeData(const ArrayBufferOrArrayBufferView&,
                                 const MojoWriteDataOptions*);
  MojoReadDataResult* queryData() const;
  MojoReadDataResult* discardData(unsigned num_bytes,
                                  const MojoDiscardDataOptions*);
  MojoReadDataResult* readData(ArrayBufferOrArrayBufferView&,
                               const MojoReadDataOptions*) const;

  // SharedBuffer handle.
  MojoMapBufferResult* mapBuffer(unsigned offset, unsigned num_bytes);
  MojoCreateSharedBufferResult* duplicateBufferHandle(
      const MojoDuplicateBufferHandleOptions*);

 private:
  mojo::ScopedHandle handle_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_MOJO_MOJO_HANDLE_H_
