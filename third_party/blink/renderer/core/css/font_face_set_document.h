/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_FONT_FACE_SET_DOCUMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_FONT_FACE_SET_DOCUMENT_H_

#include "base/macros.h"
#include "third_party/blink/renderer/bindings/core/v8/iterable.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/css/css_font_selector.h"
#include "third_party/blink/renderer/core/css/font_face.h"
#include "third_party/blink/renderer/core/css/font_face_set.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event_listener.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class Font;

class CORE_EXPORT FontFaceSetDocument final : public FontFaceSet,
                                              public Supplement<Document> {
  USING_GARBAGE_COLLECTED_MIXIN(FontFaceSetDocument);

 public:
  static const char kSupplementName[];

  explicit FontFaceSetDocument(Document&);
  ~FontFaceSetDocument() override;

  ScriptPromise ready(ScriptState*) override;

  AtomicString status() const override;

  void DidLayout();
  void BeginFontLoading(FontFace*);

  // FontFace::LoadFontCallback
  void NotifyLoaded(FontFace*) override;
  void NotifyError(FontFace*) override;

  size_t ApproximateBlankCharacterCount() const;

  static FontFaceSetDocument* From(Document&);
  static void DidLayout(Document&);
  static size_t ApproximateBlankCharacterCount(Document&);

  void Trace(blink::Visitor*) override;

 protected:
  bool InActiveContext() const override;
  FontSelector* GetFontSelector() const override {
    // TODO(Fserb): tracking down crbug.com/988125, can be DCHECK later.
    CHECK(IsMainThread());
    return GetDocument()->GetStyleEngine().GetFontSelector();
  }

  bool ResolveFontStyle(const String&, Font&) override;

 private:
  void FireDoneEventIfPossible() override;
  const HeapLinkedHashSet<Member<FontFace>>& CSSConnectedFontFaceList()
      const override;

  class FontLoadHistogram {
    DISALLOW_NEW();

   public:
    enum Status { kNoWebFonts, kHadBlankText, kDidNotHaveBlankText, kReported };
    FontLoadHistogram() : status_(kNoWebFonts) {}
    void UpdateStatus(FontFace*);
    void Record();

   private:
    Status status_;
  };
  FontLoadHistogram histogram_;
  DISALLOW_COPY_AND_ASSIGN(FontFaceSetDocument);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_FONT_FACE_SET_DOCUMENT_H_
