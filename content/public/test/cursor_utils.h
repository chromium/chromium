// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_CURSOR_UTILS_H_
#define CONTENT_PUBLIC_TEST_CURSOR_UTILS_H_

#include "content/public/browser/web_contents.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"

namespace content {
class CursorUtils {
 public:
  static ui::mojom::CursorType GetLastCursorForWebContents(
      WebContents* web_contents);
};
}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_CURSOR_UTILS_H_
