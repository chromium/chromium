// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_EVENTS_EVENT_DISPATCH_FORBIDDEN_SCOPE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_EVENTS_EVENT_DISPATCH_FORBIDDEN_SCOPE_H_

#include "base/auto_reset.h"
#include "base/check_op.h"
#include "base/dcheck_is_on.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

#if DCHECK_IS_ON()

class [[nodiscard]] EventDispatchForbiddenScope {
  STACK_ALLOCATED();

 public:
  EventDispatchForbiddenScope() {
    DCHECK(IsMainThread());
    ++count_;
    CHECK_NE(count_, 0u);
  }
  EventDispatchForbiddenScope(const EventDispatchForbiddenScope&) = delete;
  EventDispatchForbiddenScope& operator=(const EventDispatchForbiddenScope&) =
      delete;

  ~EventDispatchForbiddenScope() {
    DCHECK(IsMainThread());
    CHECK_NE(count_, 0u);
    --count_;
  }

  static bool IsEventDispatchForbidden() {
    if (!IsMainThread())
      return false;
    return count_;
  }

  class [[nodiscard]] AllowUserAgentEvents {
    STACK_ALLOCATED();

   public:
    AllowUserAgentEvents() : change_(&count_, 0) { DCHECK(IsMainThread()); }

    ~AllowUserAgentEvents() { DCHECK(!count_); }

    base::AutoReset<unsigned> change_;
  };

 private:
  CORE_EXPORT static unsigned count_;
};

#else

class [[nodiscard]] EventDispatchForbiddenScope {
  STACK_ALLOCATED();

 public:
  EventDispatchForbiddenScope() {}
  EventDispatchForbiddenScope(const EventDispatchForbiddenScope&) = delete;
  EventDispatchForbiddenScope& operator=(const EventDispatchForbiddenScope&) =
      delete;

  class [[nodiscard]] AllowUserAgentEvents {
    STACK_ALLOCATED();

   public:
    AllowUserAgentEvents() {}
  };
};

#endif  // DCHECK_IS_ON()

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_EVENTS_EVENT_DISPATCH_FORBIDDEN_SCOPE_H_
