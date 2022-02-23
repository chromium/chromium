// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_BLOCK_BREAK_TOKEN_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_BLOCK_BREAK_TOKEN_DATA_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"

namespace blink {

struct NGBlockBreakTokenData {
  enum NGBreakTokenDataType {
    kBlockBreakTokenData,
    kFlexBreakTokenData,
    kGridBreakTokenData
    // When adding new values, ensure |type| below has enough bits.
  };
  NGBreakTokenDataType Type() const {
    return static_cast<NGBreakTokenDataType>(type);
  }

  explicit NGBlockBreakTokenData(
      NGBreakTokenDataType type = kBlockBreakTokenData,
      const NGBlockBreakTokenData* other_data = nullptr)
      : type(type) {
    if (other_data) {
      consumed_block_size = other_data->consumed_block_size;
      consumed_block_size_legacy_adjustment =
          other_data->consumed_block_size_legacy_adjustment;
      sequence_number = other_data->sequence_number;
    }
  }

  virtual ~NGBlockBreakTokenData() = default;

  bool IsFlexType() const { return Type() == kFlexBreakTokenData; }
  bool IsGridType() const { return Type() == kGridBreakTokenData; }

  LayoutUnit consumed_block_size;
  LayoutUnit consumed_block_size_legacy_adjustment;

  unsigned sequence_number = 0;
  unsigned type : 2;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_BLOCK_BREAK_TOKEN_DATA_H_
