// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/font_face_source.h"

#include "third_party/blink/renderer/core/css/font_face_set_document.h"
#include "third_party/blink/renderer/core/css/font_face_set_worker.h"

namespace blink {

FontFaceSet* FontFaceSource::fonts(Document& document) {
  return FontFaceSetDocument::From(document);
}

FontFaceSet* FontFaceSource::fonts(WorkerGlobalScope& worker) {
  return FontFaceSetWorker::From(worker);
}

}  // namespace blink
