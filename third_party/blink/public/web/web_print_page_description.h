// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_PRINT_PAGE_DESCRIPTION_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_PRINT_PAGE_DESCRIPTION_H_

#include "third_party/blink/public/common/css/page_orientation.h"
#include "third_party/blink/public/platform/web_double_size.h"

namespace blink {

// Description of a specific page when printing. All sizes are in pixels.
struct WebPrintPageDescription {
  WebDoubleSize size;
  int margin_top = 0;
  int margin_right = 0;
  int margin_bottom = 0;
  int margin_left = 0;
  PageOrientation orientation = PageOrientation::kUpright;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_PRINT_PAGE_DESCRIPTION_H_
