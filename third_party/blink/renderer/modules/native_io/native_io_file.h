// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_NATIVE_IO_NATIVE_IO_FILE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_NATIVE_IO_NATIVE_IO_FILE_H_

#include <memory>

#include "base/files/file.h"
#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "third_party/blink/public/mojom/native_io/native_io.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer_view_helpers.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/modules/native_io/native_io_capacity_tracker.h"
#include "third_party/blink/renderer/modules/native_io/native_io_file_utils.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/cross_thread_handle.h"
#include "third_party/blink/renderer/platform/heap/cross_thread_persistent.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace blink {

class ExecutionContext;
class ScriptPromiseResolver;
class ScriptState;

class NativeIOFile final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  NativeIOFile(base::File backing_file,
               int64_t backing_file_length,
               HeapMojoRemote<mojom::blink::NativeIOFileHost> backend_file,
               NativeIOCapacityTracker* capacity_tracker,
               ExecutionContext*);

  NativeIOFile(const NativeIOFile&) = delete;
  NativeIOFile& operator=(const NativeIOFile&) = delete;

  // Needed because of the mojo::Remote<mojom::blink::NativeIOFile>.
  ~NativeIOFile() override;

  ScriptPromise close(ScriptState*);
  ScriptPromise getLength(ScriptState*, ExceptionState&);
  ScriptPromise setLength(ScriptState*, uint64_t new_length, ExceptionState&);
  ScriptPromise read(ScriptState*,
                     NotShared<DOMArrayBufferView> buffer,
                     uint64_t file_offset,
                     ExceptionState&);
  ScriptPromise write(ScriptState*,
                      NotShared<DOMArrayBufferView> buffer,
                      uint64_t file_offset,
                      ExceptionState&);
  ScriptPromise flush(ScriptState*, ExceptionState&);

  // GarbageCollected
  void Trace(Visitor* visitor) const override;

 private:
  class FileState;

  // Called when the mojo backend disconnects.
  void OnBackendDisconnect();

  // Runs any queued close operation.
  void DispatchQueuedClose();

  // Performs the file I/O part of close().
  static void DoClose(
      CrossThreadHandle<NativeIOFile> native_io_file,
      CrossThreadHandle<ScriptPromiseResolver> resolver,
      scoped_refptr<NativeIOFile::FileState> file_state,
      scoped_refptr<base::SequencedTaskRunner> file_task_runner);
  // Performs the post file I/O part of close(), on the main thread.
  void DidClose(ScriptPromiseResolver* resolver);

  // Performs the file I/O part of getLength(), off the main thread.
  static void DoGetLength(
      CrossThreadHandle<NativeIOFile> native_io_file,
      CrossThreadHandle<ScriptPromiseResolver> resolver,
      scoped_refptr<NativeIOFile::FileState> file_state,
      scoped_refptr<base::SequencedTaskRunner> file_task_runner);
  // Performs the post file I/O part of getLength(), on the main thread.
  void DidGetLength(ScriptPromiseResolver* resolver,
                    int64_t length,
                    base::File::Error get_length_error);

  // Performs the file I/O part of getLength(), off the main thread.
  static void DoSetLength(
      CrossThreadHandle<NativeIOFile> native_io_file,
      CrossThreadHandle<ScriptPromiseResolver> resolver,
      scoped_refptr<NativeIOFile::FileState> file_state,
      scoped_refptr<base::SequencedTaskRunner> file_task_runner,
      int64_t expected_length);
  // Performs the post file I/O part of setLength(), on the main thread.
  //
  // `actual_length` is negative if the I/O operation was unsuccessful and the
  // correct length of the file could not be determined.
  void DidSetLengthIo(ScriptPromiseResolver* resolver,
                      int64_t actual_length,
                      base::File::Error set_length_result);
#if BUILDFLAG(IS_MAC)
  // Performs the post IPC part of setLength(), on the main thread.
  //
  // `actual_length` is negative if the I/O operation was unsuccessful and the
  // correct length of the file could not be determined.
  void DidSetLengthIpc(ScriptPromiseResolver* resolver,
                       base::File backing_file,
                       int64_t actual_length,
                       mojom::blink::NativeIOErrorPtr set_length_result);
#endif  // BUILDFLAG(IS_MAC)

  // Performs the file I/O part of read(), off the main thread.
  static void DoRead(CrossThreadHandle<NativeIOFile> native_io_file,
                     CrossThreadHandle<ScriptPromiseResolver> resolver,
                     scoped_refptr<NativeIOFile::FileState> file_state,
                     scoped_refptr<base::SequencedTaskRunner> file_task_runner,
                     std::unique_ptr<NativeIODataBuffer> result_buffer_data,
                     uint64_t file_offset,
                     int read_size);
  // Performs the post file I/O part of read(), on the main thread.
  void DidRead(ScriptPromiseResolver* resolver,
               std::unique_ptr<NativeIODataBuffer> result_buffer_data,
               int read_bytes,
               base::File::Error read_error);

  // Performs the file I/O part of write(), off the main thread.
  static void DoWrite(
      CrossThreadHandle<NativeIOFile> native_io_file,
      CrossThreadHandle<ScriptPromiseResolver> resolver,
      scoped_refptr<NativeIOFile::FileState> file_state,
      scoped_refptr<base::SequencedTaskRunner> resolver_task_runner,
      std::unique_ptr<NativeIODataBuffer> result_buffer_data,
      uint64_t file_offset,
      int write_size);
  // Performs the post file I/O part of write(), on the main thread.
  //
  // `actual_file_length_on_failure` is negative if the I/O operation was
  // unsuccessful and the correct length of the file could not be determined.
  void DidWrite(ScriptPromiseResolver* resolver,
                std::unique_ptr<NativeIODataBuffer> result_buffer_data,
                int written_bytes,
                base::File::Error write_error,
                int write_size,
                int64_t actual_file_length_on_failure);

  // Performs the file I/O part of flush().
  static void DoFlush(
      CrossThreadHandle<NativeIOFile> native_io_file,
      CrossThreadHandle<ScriptPromiseResolver> resolver,
      scoped_refptr<NativeIOFile::FileState> file_state,
      scoped_refptr<base::SequencedTaskRunner> file_task_runner);
  // Performs the post file-I/O part of flush(), on the main thread.
  void DidFlush(ScriptPromiseResolver* resolver, base::File::Error flush_error);

  // Kicks off closing the file from the main thread.
  void CloseBackingFile();

  // True when an I/O operation other than close is underway.
  //
  // Checked before kicking off any I/O operation except for close(). This
  // ensures that at most one I/O operation is underway at any given
  // time. |io_pending_| is meant to only be accessed on the main (JS)
  // thread. At a high level, it's true when the mutex guarding the actual file
  // object (within |file_state_|) is held, but it's also true during thread
  // hops, when the mutex is not held.
  bool io_pending_ = false;

  // Non-null when a close() I/O is queued behind another I/O operation.
  //
  // Set when close() is called while another I/O operation is underway. Cleared
  // when the queued close() operation is queued.
  Member<ScriptPromiseResolver> queued_close_resolver_;

  // Set to true when close() is called, or when the backend goes away.
  bool closed_ = false;

  // The length of the file used in capacity accounting. Most of the time, this
  // is the file's length. When `io_pending_` is true, `file_length_` may be
  // larger than the actual length, to reflect capacity allocated for an
  // in-progress I/O operation, or capacity that will be released by an
  // in-progress I/O operation.
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
  //
  // TODO(rstz): Consider moving this variable into `file_state_`
  int64_t file_length_ = 0;

  // Points to a NativeIOFile::FileState while the underlying file is open.
  //
  // When the underlying file is closed, this pointer is nulled out, and the
  // FileState instance is passed to a different thread, where the closing
  // happens. This avoids having any I/O performed by the base::File::Close()
  // jank the JavaScript thread that owns this NativeIOFile instance.
  scoped_refptr<FileState> file_state_;

  // Schedules resolving Promises with file I/O results.
  const scoped_refptr<base::SequencedTaskRunner> resolver_task_runner_;

  // Mojo pipe that holds the renderer's lock on the file.
  HeapMojoRemote<mojom::blink::NativeIOFileHost> backend_file_;

  // Tracks the capacity for this file's execution context.
  Member<NativeIOCapacityTracker> capacity_tracker_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_NATIVE_IO_NATIVE_IO_FILE_H_
