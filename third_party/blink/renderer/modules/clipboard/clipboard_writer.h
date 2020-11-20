// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CLIPBOARD_CLIPBOARD_WRITER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CLIPBOARD_CLIPBOARD_WRITER_H_

#include "base/sequence_checker.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/core/fileapi/file_reader_loader_client.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/self_keep_alive.h"
#include "third_party/skia/include/core/SkImage.h"

namespace blink {

class ClipboardPromise;
class FileReaderLoader;
class SystemClipboard;
class RawSystemClipboard;

// Interface for writing async-clipboard-compatible types as a Blob to the
// System Clipboard, asynchronously.
//
// Writing a Blob's data to the system clipboard is accomplished by:
// (1) Reading the blob's contents using a FileReaderLoader.
// (2) Decoding the blob's contents to avoid RCE in native applications that may
//     take advantage of vulnerabilities in their decoders. In
//     ClipboardRawDataWriter, this decoding is skipped.
// (3) Writing the blob's decoded contents to the system clipboard.
//
// ClipboardWriter is owned only by itself and ClipboardPromise. It keeps
// itself alive for the duration of FileReaderLoader's async operations using
// SelfKeepAlive, and keeps itself alive afterwards during cross-thread
// operations by using WrapCrossThreadPersistent.
class ClipboardWriter : public GarbageCollected<ClipboardWriter>,
                        public FileReaderLoaderClient {
 public:
  static ClipboardWriter* Create(SystemClipboard* system_clipboard,
                                 const String& mime_type,
                                 ClipboardPromise* promise);
  static ClipboardWriter* Create(RawSystemClipboard* raw_system_clipboard,
                                 const String& mime_type,
                                 ClipboardPromise* promise);
  static ClipboardWriter* Create(SystemClipboard* system_clipboard,
                                 const String& mime_type,
                                 ClipboardPromise* promise,
                                 LocalFrame* frame);
  ~ClipboardWriter() override;

  static bool IsValidType(const String& type, bool is_raw);
  void WriteToSystem(Blob*);

  // FileReaderLoaderClient.
  void DidStartLoading() override;
  void DidReceiveData() override;
  void DidFinishLoading() override;
  void DidFail(FileErrorCode) override;

  void Trace(Visitor*) const;

 protected:
  ClipboardWriter(SystemClipboard* system_clipboard, ClipboardPromise* promise);
  ClipboardWriter(RawSystemClipboard* raw_system_clipboard,
                  ClipboardPromise* promise);

  virtual void StartWrite(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      DOMArrayBuffer* raw_data) = 0;
  virtual void DecodeOnBackgroundThread(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      DOMArrayBuffer* raw_data) = 0;
  // This ClipboardPromise owns this.
  Member<ClipboardPromise> promise_;

  // Ensure that System Clipboard operations occur on the main thread.
  SEQUENCE_CHECKER(sequence_checker_);

  SystemClipboard* system_clipboard() { return system_clipboard_; }
  RawSystemClipboard* raw_system_clipboard() { return raw_system_clipboard_; }

 private:
  ClipboardWriter(SystemClipboard* system_clipboard,
                  RawSystemClipboard* raw_system_clipboard,
                  ClipboardPromise* promise);
  // TaskRunner for interacting with the system clipboard.
  const scoped_refptr<base::SingleThreadTaskRunner> clipboard_task_runner_;
  // TaskRunner for reading files.
  const scoped_refptr<base::SingleThreadTaskRunner> file_reading_task_runner_;
  // This FileReaderLoader will load the Blob.
  std::unique_ptr<FileReaderLoader> file_reader_;
  // Access to the global sanitized system clipboard.
  Member<SystemClipboard> system_clipboard_;
  // Access to the global unsanitized system clipboard.
  Member<RawSystemClipboard> raw_system_clipboard_;

  // Oilpan: ClipboardWriter must remain alive until Member<T>::Clear() is
  // called, to keep the FileReaderLoader alive and avoid unexpected UaPs.
  SelfKeepAlive<ClipboardWriter> self_keep_alive_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CLIPBOARD_CLIPBOARD_WRITER_H_
