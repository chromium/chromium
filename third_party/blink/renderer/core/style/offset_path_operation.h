// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_OFFSET_PATH_OPERATION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_OFFSET_PATH_OPERATION_H_

#include "third_party/blink/renderer/platform/wtf/ref_counted.h"

namespace blink {

class OffsetPathOperation : public RefCounted<OffsetPathOperation> {
  USING_FAST_MALLOC(OffsetPathOperation);

 public:
  enum OperationType { kReference, kShape };

  OffsetPathOperation(const OffsetPathOperation&) = delete;
  OffsetPathOperation& operator=(const OffsetPathOperation&) = delete;
  virtual ~OffsetPathOperation() = default;

  bool operator==(const OffsetPathOperation& o) const {
    return IsSameType(o) && IsEqualAssumingSameType(o);
  }
  bool operator!=(const OffsetPathOperation& o) const { return !(*this == o); }

  virtual OperationType GetType() const = 0;
  bool IsSameType(const OffsetPathOperation& o) const {
    return o.GetType() == GetType();
  }

 protected:
  OffsetPathOperation() = default;
  virtual bool IsEqualAssumingSameType(const OffsetPathOperation& o) const = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_OFFSET_PATH_OPERATION_H_
