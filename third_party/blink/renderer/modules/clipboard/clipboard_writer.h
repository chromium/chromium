// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CLIPBOARD_CLIPBOARD_WRITER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CLIPBOARD_CLIPBOARD_WRITER_H_

#include "base/sequence_checker.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/core/fileapi/file_reader_client.h"
#include "third_party/blink/renderer/modules/clipboard/clipboard_promise.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/self_keep_alive.h"
#include "third_party/skia/include/core/SkImage.h"

namespace blink {

class FileReaderLoader;
class SystemClipboard;

// Interface for writing an individual Clipboard API format as a Blob or
// String to the System Clipboard, safely and asynchronously.
//
// ClipboardWriter takes as input a ClipboardPromise, which manages writing
// multiple formats and passes in unsanitized clipboard payloads.
// ClipboardWriter then sanitizes a Blob payload and writes it onto the
// underlying system clipboard. All System Clipboard operations should be
// called from the main thread.
//
// Writing a Blob's data to the system clipboard is accomplished by:
// (1) Reading - the Blob's contents using a FileReaderLoader.
// (2) Decoding - or sanitizing the Blob's contents to avoid RCE in native
//     applications that may take advantage of vulnerabilities in their
//     decoders, whenever possible. Decoding may be time-consuming, so it
//     is done on a background thread whenever possible. An example where
//     decoding is done on the main thread is HTML, where Blink's HTML decoder
//     can only be used on the main thread.
// (3) Writing - the Blob's decoded contents to the system clipboard.
//
// Format-specific writer classes inherit from ClipboardWriter and implement
// the FileReaderClient interface. The architecture includes:
// - ClipboardStringWriter: Base class for text-based formats (HTML, SVG, plain
// text)
// - ClipboardImageWriter: Handles binary image data with background decoding
// - ClipboardCustomFormatWriter: Handles arbitrary unsanitized content
//
// Each writer class:
// (1) Implements DidFinishLoading() to handle FileReaderLoader completion
// (2) For binary data (images): Uses background thread decoding via
//     DecodeOnBackgroundThread() and cross-thread task posting
// (3) For string data: Processes content on main thread and writes directly
//     to system clipboard via WriteString() override
//
// ClipboardWriter is owned only by itself and ClipboardPromise. It keeps
// itself alive for the duration of FileReaderLoader's async operations using
// SelfKeepAlive, and keeps itself alive afterwards during cross-thread
// operations by using WrapCrossThreadPersistent.
class ClipboardWriter : public GarbageCollected<ClipboardWriter>,
                        public FileReaderAccumulator {
 public:
  static ClipboardWriter* Create(SystemClipboard* system_clipboard,
                                 const String& mime_type,
                                 ClipboardPromise* promise);

  ~ClipboardWriter() override;

  // Begins the sequence of writing the ClipboardItemData to the system
  // clipboard.
  void WriteToSystem(V8UnionBlobOrString* clipboard_item_data);

  virtual void WriteString(String text);

  // FileReaderClient.
  void DidFinishLoading(FileReaderData) override = 0;
  void DidFail(FileErrorCode) override;

  // Common helper for DidFinishLoading implementations
  bool CleanupAfterFileReaderFinishedAndCheckIfCanProceed();

  void Trace(Visitor*) const override;

 protected:
  ClipboardWriter(SystemClipboard* system_clipboard, ClipboardPromise* promise);

  // SystemClipboard is bound to LocalFrame, so the bound LocalFrame must still
  // be valid by the time it's used.
  SystemClipboard* system_clipboard() {
    DCHECK(promise_->GetLocalFrame());
    return system_clipboard_.Get();
  }

  scoped_refptr<base::SingleThreadTaskRunner> clipboard_task_runner() {
    return clipboard_task_runner_;
  }

  // This ClipboardPromise owns this ClipboardWriter. Subclasses use `promise_`
  // to report success or failure, or to obtain the execution context.
  Member<ClipboardPromise> promise_;

  // Every subclass method that runs on the main thread should
  // DCHECK_CALLED_ON_VALID_SEQUENCE with this checker.
  SEQUENCE_CHECKER(sequence_checker_);

 private:
  // TaskRunner for interacting with the system clipboard.
  const scoped_refptr<base::SingleThreadTaskRunner> clipboard_task_runner_;
  // TaskRunner for reading files.
  const scoped_refptr<base::SingleThreadTaskRunner> file_reading_task_runner_;
  // This FileReaderLoader will load the Blob.
  Member<FileReaderLoader> file_reader_;
  // Access to the global sanitized system clipboard.
  Member<SystemClipboard> system_clipboard_;
  // Oilpan: ClipboardWriter must remain alive until Member<T>::Clear() is
  // called, to keep the FileReaderLoader alive and avoid unexpected UaPs.
  SelfKeepAlive<ClipboardWriter> self_keep_alive_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CLIPBOARD_CLIPBOARD_WRITER_H_
