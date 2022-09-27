// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_MEMBER_COPY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_MEMBER_COPY_H_

#include <memory>
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/style/content_data.h"
#include "third_party/blink/renderer/core/style/data_ref.h"
#include "third_party/blink/renderer/core/style/style_filter_data.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

template <typename T>
scoped_refptr<T> MemberCopy(const scoped_refptr<T>& v) {
  return v;
}

template <typename T>
Persistent<T> MemberCopy(const Persistent<T>& v) {
  return v;
}

template <typename T>
std::unique_ptr<T> MemberCopy(const std::unique_ptr<T>& v) {
  return v ? v->Clone() : nullptr;
}

inline Persistent<ContentData> MemberCopy(const Persistent<ContentData>& v) {
  return v ? v->Clone() : nullptr;
}

inline Persistent<StyleFilterData> MemberCopy(
    const Persistent<StyleFilterData>& v) {
  return v->Copy();
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_MEMBER_COPY_H_
