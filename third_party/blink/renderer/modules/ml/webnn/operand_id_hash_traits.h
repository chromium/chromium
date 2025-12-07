// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_OPERAND_ID_HASH_TRAITS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_OPERAND_ID_HASH_TRAITS_H_

#include "services/webnn/public/cpp/webnn_types.h"
#include "third_party/blink/renderer/platform/wtf/hash_traits.h"

namespace blink {

// Specialization of HashTraits for OperandId.
template <>
struct HashTraits<webnn::OperandId> : GenericHashTraits<webnn::OperandId> {
  static unsigned GetHash(const webnn::OperandId& key) {
    return HashTraits<webnn::OperandId::underlying_type>::GetHash(key.value());
  }

  static const webnn::OperandId& EmptyValue() {
    static webnn::OperandId empty_value(
        IntWithZeroKeyHashTraits<
            webnn::OperandId::underlying_type>::EmptyValue());
    return empty_value;
  }

  static const webnn::OperandId& DeletedValue() {
    static webnn::OperandId deleted_value(
        IntWithZeroKeyHashTraits<
            webnn::OperandId::underlying_type>::DeletedValue());
    return deleted_value;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ML_WEBNN_OPERAND_ID_HASH_TRAITS_H_
