// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/page_dismissal_scope.h"

#include "base/check.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

static unsigned page_dismissal_scope_count = 0;

PageDismissalScope::PageDismissalScope() {
  DCHECK(IsMainThread());
  ++page_dismissal_scope_count;
}

PageDismissalScope::~PageDismissalScope() {
  DCHECK(IsMainThread());
  --page_dismissal_scope_count;
}

bool PageDismissalScope::IsActive() {
  DCHECK(IsMainThread());
  return page_dismissal_scope_count > 0;
}

}  // namespace blink
