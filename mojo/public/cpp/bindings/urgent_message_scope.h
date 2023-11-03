// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_URGENT_MESSAGE_SCOPE_H_
#define MOJO_PUBLIC_CPP_BINDINGS_URGENT_MESSAGE_SCOPE_H_

#include "base/auto_reset.h"
#include "base/component_export.h"
#include "base/memory/stack_allocated.h"

namespace mojo {

// `UrgentMessageScope` is used in conjunction with [[SupportsUrgent]] to tag a
// mojo message as urgent. This enables messages to be conditionally marked as
// urgent without requiring a separate method.
class COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE) UrgentMessageScope {
  STACK_ALLOCATED();

 public:
  explicit UrgentMessageScope();

  UrgentMessageScope& operator=(UrgentMessageScope&& other);
  UrgentMessageScope(UrgentMessageScope&& other);

  ~UrgentMessageScope();

  static bool IsInUrgentScope();

 private:
  base::AutoReset<bool> resetter_;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_URGENT_MESSAGE_SCOPE_H_
