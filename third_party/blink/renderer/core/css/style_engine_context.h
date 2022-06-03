// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_ENGINE_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_ENGINE_CONTEXT_H_

#include "third_party/blink/renderer/core/dom/document.h"

namespace blink {

class CORE_EXPORT StyleEngineContext {
 public:
  StyleEngineContext();
  ~StyleEngineContext() = default;
  bool AddedPendingSheetBeforeBody() const {
    return added_pending_sheet_before_body_;
  }
  void AddingPendingSheet(const Document&);

 private:
  bool added_pending_sheet_before_body_ : 1;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_ENGINE_CONTEXT_H_
