// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CLIPBOARD_SYSTEM_CLIPBOARD_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CLIPBOARD_SYSTEM_CLIPBOARD_H_

#include "third_party/blink/public/mojom/clipboard/clipboard.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class DataObject;
class Image;
class KURL;
class LocalFrame;

// SystemClipboard:
// - is a LocalFrame bounded object.
// - provides sanitized, platform-neutral read/write access to the clipboard.
// - mediates between core classes and mojom::ClipboardHost.
//
// All calls to write functions must be followed by a call to CommitWrite().
class CORE_EXPORT SystemClipboard final
    : public GarbageCollected<SystemClipboard> {
 public:
  enum SmartReplaceOption { kCanSmartReplace, kCannotSmartReplace };

  explicit SystemClipboard(LocalFrame* frame);
  SystemClipboard(const SystemClipboard&) = delete;
  SystemClipboard& operator=(const SystemClipboard&) = delete;

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
                 SmartReplaceOption = kCannotSmartReplace);

  void ReadSvg(mojom::blink::ClipboardHost::ReadSvgCallback callback);
  void WriteSvg(const String& markup);

  String ReadRTF();

  SkBitmap ReadImage(mojom::ClipboardBuffer);
  String ReadImageAsImageMarkup(mojom::blink::ClipboardBuffer);

  // Write the image and its associated tag (bookmark/HTML types).
  void WriteImageWithTag(Image*, const KURL&, const String& title);
  // Write the image only.
  void WriteImage(const SkBitmap&);

  String ReadCustomData(const String& type);
  void WriteDataObject(DataObject*);

  // Clipboard write functions must use CommitWrite for changes to reach
  // the OS clipboard.
  void CommitWrite();

  void CopyToFindPboard(const String& text);

  void Trace(Visitor*) const;

 private:
  bool IsValidBufferType(mojom::ClipboardBuffer);

  HeapMojoRemote<mojom::blink::ClipboardHost> clipboard_;
  // In some Linux environments, |buffer_| may equal ClipboardBuffer::kStandard
  // or kSelection.  In other platforms |buffer_| always equals
  // ClipboardBuffer::kStandard.
  mojom::ClipboardBuffer buffer_ = mojom::ClipboardBuffer::kStandard;

  // Whether the selection buffer is available on the underlying platform.
  bool is_selection_buffer_available_ = false;

};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CLIPBOARD_SYSTEM_CLIPBOARD_H_
