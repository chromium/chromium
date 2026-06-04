// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_ELEMENT_IMAGE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_ELEMENT_IMAGE_H_

#include <memory>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/graphics/canvas_child_paint_record.h"

namespace blink {

class CORE_EXPORT ElementImage final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit ElementImage(std::unique_ptr<CanvasChildPaintRecord> record);

  double width() const;
  double height() const;
  void close();

  const std::unique_ptr<CanvasChildPaintRecord>& PaintRecord() const {
    return record_;
  }
  std::unique_ptr<CanvasChildPaintRecord> TransferPaintRecord();

 private:
  std::unique_ptr<CanvasChildPaintRecord> record_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_ELEMENT_IMAGE_H_
