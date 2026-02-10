// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_DOCUMENT_RESIZE_OPTIONS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_DOCUMENT_RESIZE_OPTIONS_H_

#include "base/memory/stack_allocated.h"

struct DocumentResizeOptions {
  STACK_ALLOCATED();

 public:
  bool should_suppress_events = false;
};

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_DOCUMENT_RESIZE_OPTIONS_H_
