// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_OPENTYPE_FONT_FORMAT_CHECK_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_OPENTYPE_FONT_FORMAT_CHECK_H_

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkTypeface.h"

namespace blink {

class FontFormatCheck {
  STACK_ALLOCATED();

 public:
  FontFormatCheck(sk_sp<SkData>);
  bool IsVariableFont();
  bool IsCbdtCblcColorFont();
  bool IsColrCpalColorFont();
  bool IsSbixColorFont();
  bool IsCff2OutlineFont();
  bool IsColorFont();

  // Still needed in FontCustomPlatformData.
  enum class VariableFontSubType {
    kNotVariable,
    kVariableTrueType,
    kVariableCFF2
  };

  static VariableFontSubType ProbeVariableFont(sk_sp<SkTypeface>);

 private:
  // hb-common.h: typedef uint32_t hb_tag_t;
  Vector<uint32_t> table_tags_;
};

}  // namespace blink

#endif
