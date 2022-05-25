// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/input/context_menu_allowed_scope.h"

namespace blink {

static unsigned g_context_menu_allowed_count = 0;

ContextMenuAllowedScope::ContextMenuAllowedScope() {
  g_context_menu_allowed_count++;
}

ContextMenuAllowedScope::~ContextMenuAllowedScope() {
  DCHECK_GT(g_context_menu_allowed_count, 0U);
  g_context_menu_allowed_count--;
}

bool ContextMenuAllowedScope::IsContextMenuAllowed() {
  return g_context_menu_allowed_count;
}

}  // namespace blink
