// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/style_retain_scope.h"

#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/wtf/thread_specific.h"

namespace blink {

namespace {

StyleRetainScope** CurrentPtr() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(ThreadSpecific<StyleRetainScope*>, current,
                                  ());
  return &*current;
}

}  // namespace

StyleRetainScope::StyleRetainScope() {
  StyleRetainScope** current_ptr = CurrentPtr();
  parent_ = *current_ptr;
  *current_ptr = this;
}

StyleRetainScope::~StyleRetainScope() {
  StyleRetainScope** current_ptr = CurrentPtr();
  DCHECK_EQ(*current_ptr, this);
  *current_ptr = parent_;
}

StyleRetainScope* StyleRetainScope::Current() {
  return *CurrentPtr();
}

}  // namespace blink
