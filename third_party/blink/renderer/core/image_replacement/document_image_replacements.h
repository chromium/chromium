// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_IMAGE_REPLACEMENT_DOCUMENT_IMAGE_REPLACEMENTS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_IMAGE_REPLACEMENT_DOCUMENT_IMAGE_REPLACEMENTS_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {
class HTMLImageElement;
class ImageReplacement;

// Associates each ImageReplacement with an HTMLImageElement (1:1 mapping),
// allowing for lookup from an HTMLImageElement.
class CORE_EXPORT DocumentImageReplacements
    : public GarbageCollected<DocumentImageReplacements>,
      public Supplement<Document> {
 public:
  static const char kSupplementName[];
  static DocumentImageReplacements* FromIfExists(Document&);
  static DocumentImageReplacements& From(Document&);

  explicit DocumentImageReplacements(Document&);

  void RegisterImageReplacement(HTMLImageElement*, ImageReplacement*);
  ImageReplacement* UnregisterImageReplacement(HTMLImageElement*);
  ImageReplacement* GetImageReplacement(HTMLImageElement*) const;

  void Trace(Visitor*) const override;

 private:
  HeapHashMap<WeakMember<HTMLImageElement>, Member<ImageReplacement>>
      replacements_;
};

}  // namespace blink
#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_IMAGE_REPLACEMENT_DOCUMENT_IMAGE_REPLACEMENTS_H_
