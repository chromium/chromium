// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_FONT_FACE_SOURCE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_FONT_FACE_SOURCE_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class Document;
class FontFaceSet;
class WorkerGlobalScope;

class CORE_EXPORT FontFaceSource {
  STATIC_ONLY(FontFaceSource);

 public:
  static FontFaceSet* fonts(Document&);
  static FontFaceSet* fonts(WorkerGlobalScope&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_FONT_FACE_SOURCE_H_
