// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_FORMATTED_TEXT_FORMATTED_TEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_FORMATTED_TEXT_FORMATTED_TEXT_H_

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/formatted_text/formatted_text_run.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/prefinalizer.h"
#include "third_party/blink/renderer/platform/text/bidi_resolver.h"
#include "third_party/blink/renderer/platform/text/bidi_text_run.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/text/text_run_iterator.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class Document;
class FontDescription;
class LayoutBlockFlow;
class V8UnionFormattedTextRunOrFormattedTextRunOrStringSequenceOrString;

class MODULES_EXPORT FormattedText final : public ScriptWrappable,
                                           public FormattedTextStyle {
  DEFINE_WRAPPERTYPEINFO();
  USING_PRE_FINALIZER(FormattedText, Dispose);

 public:
  explicit FormattedText(ExecutionContext* execution_context);
  FormattedText(const FormattedText&) = delete;
  FormattedText& operator=(const FormattedText&) = delete;

  void Trace(Visitor* visitor) const override;

  static FormattedText* format(
      ExecutionContext* execution_context,
      V8UnionFormattedTextRunOrFormattedTextRunOrStringSequenceOrString* text,
      const String& style,
      ExceptionState& exception_state);
  static FormattedText* format(
      ExecutionContext* execution_context,
      V8UnionFormattedTextRunOrFormattedTextRunOrStringSequenceOrString* text,
      const String& style,
      double inline_constraint,
      ExceptionState& exception_state);
  static FormattedText* format(
      ExecutionContext* execution_context,
      V8UnionFormattedTextRunOrFormattedTextRunOrStringSequenceOrString* text,
      const String& style,
      double inline_constraint,
      double block_constraint,
      ExceptionState& exception_state);

  bool CheckViewExists(ExceptionState* exception_state) const;

  FormattedTextRunInternal* AppendRun(FormattedTextRunInternal* run,
                                      ExceptionState& exception_state);

  sk_sp<PaintRecord> PaintFormattedText(Document& document,
                                        const FontDescription& font,
                                        double x,
                                        double y,
                                        gfx::RectF& bounds,
                                        ExceptionState& exception_state);

  void Dispose();

 private:
  static FormattedText* FormatImpl(
      ExecutionContext* execution_context,
      V8UnionFormattedTextRunOrFormattedTextRunOrStringSequenceOrString* text,
      const String& style,
      LayoutUnit inline_constraint,
      LayoutUnit block_constraint,
      ExceptionState& exception_state);

  void UpdateComputedStylesIfNeeded(Document& document,
                                    const FontDescription& defaultFont);

 private:
  HeapVector<Member<FormattedTextRunInternal>> text_runs_;
  Member<LayoutBlockFlow> block_;

  LayoutUnit inline_constraint_ = kIndefiniteSize;
  LayoutUnit block_constraint_ = kIndefiniteSize;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_FORMATTED_TEXT_FORMATTED_TEXT_H_
