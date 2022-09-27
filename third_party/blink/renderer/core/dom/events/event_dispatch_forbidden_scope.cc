// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/events/event_dispatch_forbidden_scope.h"

namespace blink {

#if DCHECK_IS_ON()
unsigned EventDispatchForbiddenScope::count_ = 0;
#endif  // DECHECK_IS_ON()

}  // namespace blink
