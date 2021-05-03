// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_FORWARD_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_FORWARD_H_

#include "third_party/blink/renderer/platform/wtf/buildflags.h"

#if BUILDFLAG(USE_V8_OILPAN)

namespace cppgc {
class Visitor;
}

namespace blink {

using Visitor = cppgc::Visitor;

}  // namespace blink

#else  // !USE_V8_OILPAN

namespace blink {

class Visitor;

}  // namespace blink

#endif  // !USE_V8_OILPAN

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_FORWARD_H_
