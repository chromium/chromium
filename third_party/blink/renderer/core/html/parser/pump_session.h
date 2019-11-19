// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_PUMP_SESSION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_PUMP_SESSION_H_

#include "third_party/blink/renderer/core/html/parser/nesting_level_incrementer.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class PumpSession : public NestingLevelIncrementer {
  STACK_ALLOCATED();

 public:
  PumpSession(unsigned& nesting_level);
  ~PumpSession();
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_PUMP_SESSION_H_
