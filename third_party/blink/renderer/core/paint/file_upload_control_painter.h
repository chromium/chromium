// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_FILE_UPLOAD_CONTROL_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_FILE_UPLOAD_CONTROL_PAINTER_H_

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class LayoutFileUploadControl;
struct PaintInfo;
struct PhysicalOffset;

class FileUploadControlPainter {
  STACK_ALLOCATED();

 public:
  FileUploadControlPainter(
      const LayoutFileUploadControl& layout_file_upload_control)
      : layout_file_upload_control_(layout_file_upload_control) {}

  void PaintObject(const PaintInfo&, const PhysicalOffset& paint_offset);

 private:
  const LayoutFileUploadControl& layout_file_upload_control_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_FILE_UPLOAD_CONTROL_PAINTER_H_
