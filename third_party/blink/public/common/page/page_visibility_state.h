// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_PAGE_PAGE_VISIBILITY_STATE_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_PAGE_PAGE_VISIBILITY_STATE_H_

namespace blink {

enum class PageVisibilityState {
  kVisible,
  kHidden,
  kHiddenButPainting,

  kMaxValue = kHiddenButPainting,
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_PAGE_PAGE_VISIBILITY_STATE_H_
