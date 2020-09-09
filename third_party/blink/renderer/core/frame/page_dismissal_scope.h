// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_PAGE_DISMISSAL_SCOPE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_PAGE_DISMISSAL_SCOPE_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class CORE_EXPORT PageDismissalScope final {
  STACK_ALLOCATED();

 public:
  PageDismissalScope();
  ~PageDismissalScope();

  static bool IsActive();

  DISALLOW_COPY_AND_ASSIGN(PageDismissalScope);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_PAGE_DISMISSAL_SCOPE_H_
