// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CLIPBOARD_CLIPBOARD_READER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CLIPBOARD_CLIPBOARD_READER_H_

#include "third_party/blink/renderer/core/fileapi/blob.h"

namespace blink {

// Interface for reading async-clipboard-compatible types from the System
// Clipboard as a Blob.
//
// Reading a type from the system clipboard to a Blob is accomplished by:
// (1) Reading the item from the system clipboard.
// (2) Encoding the blob's contents.
// (3) Writing the contents to a blob.
class ClipboardReader {
 public:
  static std::unique_ptr<ClipboardReader> Create(const String& mime_type);
  virtual ~ClipboardReader();

  virtual Blob* ReadFromSystem() = 0;

 protected:
  ClipboardReader();
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CLIPBOARD_CLIPBOARD_READER_H_
