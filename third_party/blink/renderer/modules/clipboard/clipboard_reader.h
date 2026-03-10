// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CLIPBOARD_CLIPBOARD_READER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CLIPBOARD_CLIPBOARD_READER_H_

#include "base/sequence_checker.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class ExecutionContext;
class LocalFrame;
class SystemClipboard;

class ClipboardReaderResultHandler : public GarbageCollectedMixin {
 public:
  virtual void OnRead(Blob* blob, const String& mime_type) = 0;
  virtual ExecutionContext* GetExecutionContext() const = 0;
  virtual LocalFrame* GetLocalFrame() const = 0;
  void Trace(Visitor* visitor) const override {}
};

// Interface for reading an individual Clipboard API format from the sanitized
// System Clipboard as a Blob.
//
// Reading a type from the system clipboard to a Blob is accomplished by:
// (1) Reading - the format from the system clipboard.
// (2) Encoding - the system clipboard's contents for a format. Encoding may be
//     time-consuming, so it is done on a background thread whenever possible.
//     An example where encoding is done on the main thread is HTML, where
//     Blink's HTML encoder can only be used on the main thread.
// (3) Writing - the encoded contents to a Blob.
//
// ClipboardReader takes as input a ClipboardReaderResultHandler, from which it
// can obtain context/frame access, and to which it provides a Blob containing
// an encoded SystemClipboard-originated clipboard payload. All System
// Clipboard operations should be called from the main thread.
//
// Subclasses of ClipboardReader should be implemented for each supported
// format. Subclasses should:
// (1) Begin execution by implementing ClipboardReader::Read().
// (2) Encode the payload on a background thread (if possible) by implementing
//     a static EncodeOnBackgroundThread() function. This function is called by
//     Read() via worker_pool::PostTask().
// (3) Create a Blob and send it to the result handler by implementing
//     ClipboardReader::NextRead().
class ClipboardReader : public GarbageCollected<ClipboardReader> {
 public:
  static ClipboardReader* Create(SystemClipboard* system_clipboard,
                                 const String& mime_type,
                                 ClipboardReaderResultHandler* result_handler,
                                 bool sanitize_html);
  virtual ~ClipboardReader();

  // Reads from the system clipboard and encodes on a background thread.
  virtual void Read() = 0;

  void Trace(Visitor* visitor) const;

 protected:
  // TaskRunner for interacting with the system clipboard.
  const scoped_refptr<base::SingleThreadTaskRunner> clipboard_task_runner_;

  ClipboardReader(SystemClipboard* system_clipboard,
                  ClipboardReaderResultHandler* result_handler);
  // Send a Blob holding `utf8_bytes` to the owning result handler.
  // An empty `utf8_bytes` indicates that the encoding step failed.
  virtual void NextRead(Vector<uint8_t> utf8_bytes) = 0;

  SystemClipboard* system_clipboard() { return system_clipboard_.Get(); }
  // The result handler receives Blob output and context access for readers.
  Member<ClipboardReaderResultHandler> result_handler_;

  // Every subclass method that runs on the main thread should
  // DCHECK_CALLED_ON_VALID_SEQUENCE with this checker.
  SEQUENCE_CHECKER(sequence_checker_);

 private:
  // Access to the global sanitized system clipboard.
  Member<SystemClipboard> system_clipboard_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CLIPBOARD_CLIPBOARD_READER_H_
