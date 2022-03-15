// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PENDING_SHEET_TYPE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PENDING_SHEET_TYPE_H_

#include <stdint.h>

namespace blink {

enum class PendingSheetType { kNone, kNonBlocking, kBlocking };

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PENDING_SHEET_TYPE_H_
