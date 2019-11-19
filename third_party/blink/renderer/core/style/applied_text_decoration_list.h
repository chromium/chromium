// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_APPLIED_TEXT_DECORATION_LIST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_APPLIED_TEXT_DECORATION_LIST_H_

#include "third_party/blink/renderer/core/style/applied_text_decoration.h"

namespace blink {

typedef base::RefCountedData<WTF::Vector<AppliedTextDecoration>>
    AppliedTextDecorationList;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_APPLIED_TEXT_DECORATION_LIST_H_
