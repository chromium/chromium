// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_SCROLL_TYPES_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_SCROLL_TYPES_H_

namespace blink {

enum WebScrollDirection {
  kScrollUpIgnoringWritingMode,
  kFirstScrollDirection = kScrollUpIgnoringWritingMode,
  kScrollDownIgnoringWritingMode,
  kScrollLeftIgnoringWritingMode,
  kScrollRightIgnoringWritingMode,

  kScrollBlockDirectionBackward,
  kScrollBlockDirectionForward,
  kScrollInlineDirectionBackward,
  kScrollInlineDirectionForward,
  kLastScrollDirection = kScrollInlineDirectionForward
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_SCROLL_TYPES_H_
