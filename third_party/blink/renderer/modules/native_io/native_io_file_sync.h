// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_NATIVE_IO_NATIVE_IO_FILE_SYNC_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_NATIVE_IO_NATIVE_IO_FILE_SYNC_H_

#include <stdint.h>

#include "base/files/file.h"
#include "third_party/blink/public/mojom/native_io/native_io.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_native_io_read_result.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_native_io_write_result.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer_view_helpers.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/modules/native_io/native_io_capacity_tracker.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class ExceptionState;
class ExecutionContext;

class NativeIOFileSync final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  NativeIOFileSync(base::File backing_file,
                   int64_t backing_file_size,
                   HeapMojoRemote<mojom::blink::NativeIOFileHost> backend_file,
                   NativeIOCapacityTracker* capacity_tracker,
                   ExecutionContext*);

  NativeIOFileSync(const NativeIOFileSync&) = delete;
  NativeIOFileSync& operator=(const NativeIOFileSync&) = delete;

  // Needed because of the mojo::Remote<mojom::blink::NativeIOFile>.
  ~NativeIOFileSync() override;

  void close();
  uint64_t getLength(ExceptionState&);
  void setLength(uint64_t length, ExceptionState&);
  NativeIOReadResult* read(ScriptState* script_state,
                           NotShared<DOMArrayBufferView> buffer,
                           uint64_t file_offset,
                           ExceptionState&);
  NativeIOWriteResult* write(ScriptState* script_state,
                             NotShared<DOMArrayBufferView> buffer,
                             uint64_t file_offset,
                             ExceptionState&);
  void flush(ExceptionState&);

  // GarbageCollected
  void Trace(Visitor* visitor) const override;

 private:
  // Called when the mojo backend disconnects.
  void OnBackendDisconnect();

  // The file on disk backing this NativeIOFile.
  base::File backing_file_;

  // The length of the file used in capacity accounting. This should equal the
  // current file's length, unless that length cannot be reliably determined
  // using base::GetLength(). In the latter case, the file will be force-closed
  // to prevent further corruption.
  //
  // Operations that increase the file's length must first allocate capacity,
  // update `file_length_` to reflect the increased length, and then perform the
  // I/O. If the I/O fails, GetLength() must be used to obtain the actual file
  // length. The result must first be compared against `file_length_` to account
  // for the unused capacity, then used to update `file_length_`.
  //
  // Operations that decrease the file's length must first perform the I/O, and
  // then update `file_length_` and return freed up capacity. I/O failures can
  // be handled using the same logic as above.
  int64_t file_length_ = 0;

  // Mojo pipe that holds the renderer's lock on the file.
  HeapMojoRemote<mojom::blink::NativeIOFileHost> backend_file_;

  // Manages the capacity allocation for this file manager's execution context.
  Member<NativeIOCapacityTracker> capacity_tracker_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_NATIVE_IO_NATIVE_IO_FILE_SYNC_H_
