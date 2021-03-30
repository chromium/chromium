// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CLIPBOARD_RAW_SYSTEM_CLIPBOARD_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CLIPBOARD_RAW_SYSTEM_CLIPBOARD_H_

#include "third_party/blink/public/mojom/clipboard/raw_clipboard.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_wrapper_mode.h"

// RawSystemClipboard:
// - is a singleton.
// - provides read/write access of unsanitized, platform-specific data.
// - mediates between async clipboard and mojom::RawClipboardHost.
//
// All calls to Write() must be followed by a call to CommitWrite().
namespace blink {

class LocalFrame;

class CORE_EXPORT RawSystemClipboard final
    : public GarbageCollected<RawSystemClipboard> {
 public:
  explicit RawSystemClipboard(LocalFrame* frame);

  RawSystemClipboard(const RawSystemClipboard&) = delete;
  RawSystemClipboard& operator=(const RawSystemClipboard&) = delete;

  void ReadAvailableFormatNames(
      mojom::blink::RawClipboardHost::ReadAvailableFormatNamesCallback
          callback);

  void Read(const String& type,
            mojom::blink::RawClipboardHost::ReadCallback callback);

  void Write(const String& type, mojo_base::BigBuffer data);

  void CommitWrite();
  void Trace(Visitor*) const;

 private:
  HeapMojoRemote<mojom::blink::RawClipboardHost> clipboard_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CLIPBOARD_RAW_SYSTEM_CLIPBOARD_H_
