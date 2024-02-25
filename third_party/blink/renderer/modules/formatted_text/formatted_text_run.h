// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_FORMATTED_TEXT_FORMATTED_TEXT_RUN_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_FORMATTED_TEXT_FORMATTED_TEXT_RUN_H_

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/modules/formatted_text/formatted_text_style.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/prefinalizer.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class FormattedTextRunInternal final
    : public GarbageCollected<FormattedTextRunInternal>,
      public FormattedTextStyle {
 public:
  static FormattedTextRunInternal* Create(ExecutionContext* execution_context,
                                          const String text) {
    return MakeGarbageCollected<FormattedTextRunInternal>(execution_context,
                                                          text);
  }

  FormattedTextRunInternal(ExecutionContext*, const String text);
  FormattedTextRunInternal(const FormattedTextRunInternal&) = delete;
  FormattedTextRunInternal& operator=(const FormattedTextRunInternal&) = delete;

  LayoutText* GetLayoutObject() { return layout_text_.Get(); }
  void UpdateStyle(Document& document, const ComputedStyle& parent_style);

  void Trace(Visitor* visitor) const override;

 private:
  String text_;
  Member<LayoutText> layout_text_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_FORMATTED_TEXT_FORMATTED_TEXT_RUN_H_
