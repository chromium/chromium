// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CLIPBOARD_SYSTEM_CLIPBOARD_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CLIPBOARD_SYSTEM_CLIPBOARD_H_

#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/clipboard/clipboard.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class DataObject;
class Image;
class KURL;

// This singleton provides read/write access to the system clipboard,
// mediating between core classes and mojom::ClipboardHost.
// All calls to write functions must be followed by a call to CommitWrite().
class CORE_EXPORT SystemClipboard {
  USING_FAST_MALLOC(SystemClipboard);

 public:
  static SystemClipboard& GetInstance();

  enum SmartReplaceOption { kCanSmartReplace, kCannotSmartReplace };

  uint64_t SequenceNumber();
  bool IsSelectionMode() const;
  void SetSelectionMode(bool);
  bool CanSmartReplace();
  bool IsHTMLAvailable();
  Vector<String> ReadAvailableTypes();

  String ReadPlainText();
  String ReadPlainText(mojom::ClipboardBuffer buffer);
  void WritePlainText(const String&, SmartReplaceOption = kCannotSmartReplace);

  // If no data is read, an empty string will be returned and all out parameters
  // will be cleared. If applicable, the page URL will be assigned to the KURL
  // parameter. fragmentStart and fragmentEnd are indexes into the returned
  // markup that indicate the start and end of the returned markup. If there is
  // no additional context, fragmentStart will be zero and fragmentEnd will be
  // the same as the length of the markup.
  String ReadHTML(KURL&, unsigned& fragment_start, unsigned& fragment_end);
  void WriteHTML(const String& markup,
                 const KURL& document_url,
                 const String& plain_text,
                 SmartReplaceOption = kCannotSmartReplace);

  String ReadRTF();

  SkBitmap ReadImage(mojom::ClipboardBuffer);

  // Write the image and its associated tag (bookmark/HTML types).
  void WriteImageWithTag(Image*, const KURL&, const String& title);
  // Write the image only.
  void WriteImage(const SkBitmap&);

  String ReadCustomData(const String& type);
  void WriteDataObject(DataObject*);

  // Clipboard write functions must use CommitWrite for changes to reach
  // the OS clipboard.
  void CommitWrite();

 private:
  SystemClipboard();
  bool IsValidBufferType(mojom::ClipboardBuffer);

  mojo::Remote<mojom::blink::ClipboardHost> clipboard_;
  // In X11, |buffer_| may equal ClipboardBuffer::kStandard or kSelection.
  // Outside X11, |buffer_| always equals ClipboardBuffer::kStandard.
  mojom::ClipboardBuffer buffer_ = mojom::ClipboardBuffer::kStandard;

  DISALLOW_COPY_AND_ASSIGN(SystemClipboard);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CLIPBOARD_SYSTEM_CLIPBOARD_H_
