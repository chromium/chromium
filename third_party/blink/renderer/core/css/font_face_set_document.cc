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

#include "third_party/blink/renderer/core/css/font_face_set_document.h"

#include "third_party/blink/renderer/bindings/core/v8/dictionary.h"
#include "third_party/blink/renderer/core/css/css_font_selector.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/css_segmented_font_face.h"
#include "third_party/blink/renderer/core/css/font_face_cache.h"
#include "third_party/blink/renderer/core/css/font_face_set_load_event.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/resolver/font_style_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"

namespace blink {

// static
const char FontFaceSetDocument::kSupplementName[] = "FontFaceSetDocument";

FontFaceSetDocument::FontFaceSetDocument(Document& document)
    : FontFaceSet(document), Supplement<Document>(document) {
}

FontFaceSetDocument::~FontFaceSetDocument() = default;

bool FontFaceSetDocument::InActiveContext() const {
  ExecutionContext* context = GetExecutionContext();
  return context && To<Document>(context)->IsActive();
}


AtomicString FontFaceSetDocument::status() const {
  DEFINE_STATIC_LOCAL(AtomicString, loading, ("loading"));
  DEFINE_STATIC_LOCAL(AtomicString, loaded, ("loaded"));
  return is_loading_ ? loading : loaded;
}

void FontFaceSetDocument::DidLayout() {
  if (GetDocument()->GetFrame()->IsMainFrame() && loading_fonts_.IsEmpty())
    histogram_.Record();
  if (!ShouldSignalReady())
    return;
  HandlePendingEventsAndPromisesSoon();
}

void FontFaceSetDocument::BeginFontLoading(FontFace* font_face) {
  AddToLoadingFonts(font_face);
}

void FontFaceSetDocument::NotifyLoaded(FontFace* font_face) {
  histogram_.UpdateStatus(font_face);
  loaded_fonts_.push_back(font_face);
  RemoveFromLoadingFonts(font_face);
}

void FontFaceSetDocument::NotifyError(FontFace* font_face) {
  histogram_.UpdateStatus(font_face);
  failed_fonts_.push_back(font_face);
  RemoveFromLoadingFonts(font_face);
}

size_t FontFaceSetDocument::ApproximateBlankCharacterCount() const {
  size_t count = 0;
  for (auto& font_face : loading_fonts_)
    count += font_face->ApproximateBlankCharacterCount();
  return count;
}

ScriptPromise FontFaceSetDocument::ready(ScriptState* script_state) {
  if (ready_->GetState() != ReadyProperty::kPending && InActiveContext()) {
    // |ready_| is already resolved, but there may be pending stylesheet
    // changes and/or layout operations that may cause another font loads.
    // So synchronously update style and layout here.
    // This may trigger font loads, and replace |ready_| with a new Promise.
    GetDocument()->UpdateStyleAndLayout();
  }
  return ready_->Promise(script_state->World());
}

const HeapLinkedHashSet<Member<FontFace>>&
FontFaceSetDocument::CSSConnectedFontFaceList() const {
  Document* document = this->GetDocument();
  document->UpdateActiveStyle();
  return GetFontSelector()->GetFontFaceCache()->CssConnectedFontFaces();
}

void FontFaceSetDocument::FireDoneEventIfPossible() {
  if (should_fire_loading_event_)
    return;
  if (!ShouldSignalReady())
    return;
  Document* d = GetDocument();
  if (!d)
    return;

  // If the layout was invalidated in between when we thought layout
  // was updated and when we're ready to fire the event, just wait
  // until after the next layout before firing events.
  if (!d->View() || d->View()->NeedsLayout())
    return;

  FireDoneEvent();
}


bool FontFaceSetDocument::ResolveFontStyle(const String& font_string,
                                           Font& font) {
  if (font_string.IsEmpty())
    return false;

  // Interpret fontString in the same way as the 'font' attribute of
  // CanvasRenderingContext2D.
  auto* parsed_style =
      MakeGarbageCollected<MutableCSSPropertyValueSet>(kHTMLStandardMode);
  CSSParser::ParseValue(parsed_style, CSSPropertyID::kFont, font_string, true,
                        GetDocument()->GetSecureContextMode());
  if (parsed_style->IsEmpty())
    return false;

  String font_value = parsed_style->GetPropertyValue(CSSPropertyID::kFont);
  if (font_value == "inherit" || font_value == "initial")
    return false;

  if (!GetDocument()->documentElement()) {
    auto* font_selector = GetDocument()->GetStyleEngine().GetFontSelector();
    FontDescription description =
        FontStyleResolver::ComputeFont(*parsed_style, font_selector);
    font = Font(description);
    font.Update(font_selector);
    return true;
  }

  scoped_refptr<ComputedStyle> style = ComputedStyle::Create();

  FontFamily font_family;
  font_family.SetFamily(FontFaceSet::kDefaultFontFamily);

  FontDescription default_font_description;
  default_font_description.SetFamily(font_family);
  default_font_description.SetSpecifiedSize(FontFaceSet::kDefaultFontSize);
  default_font_description.SetComputedSize(FontFaceSet::kDefaultFontSize);

  style->SetFontDescription(default_font_description);

  style->GetFont().Update(style->GetFont().GetFontSelector());

  GetDocument()->UpdateActiveStyle();
  GetDocument()->EnsureStyleResolver().ComputeFont(
      *GetDocument()->documentElement(), style.get(), *parsed_style);

  font = style->GetFont();
  font.Update(GetFontSelector());
  return true;
}

FontFaceSetDocument* FontFaceSetDocument::From(Document& document) {
  FontFaceSetDocument* fonts =
      Supplement<Document>::From<FontFaceSetDocument>(document);
  if (!fonts) {
    fonts = MakeGarbageCollected<FontFaceSetDocument>(document);
    Supplement<Document>::ProvideTo(document, fonts);
  }

  return fonts;
}

void FontFaceSetDocument::DidLayout(Document& document) {
  if (FontFaceSetDocument* fonts =
          Supplement<Document>::From<FontFaceSetDocument>(document))
    fonts->DidLayout();
}

size_t FontFaceSetDocument::ApproximateBlankCharacterCount(Document& document) {
  if (FontFaceSetDocument* fonts =
          Supplement<Document>::From<FontFaceSetDocument>(document))
    return fonts->ApproximateBlankCharacterCount();
  return 0;
}

void FontFaceSetDocument::Trace(blink::Visitor* visitor) {
  Supplement<Document>::Trace(visitor);
  FontFaceSet::Trace(visitor);
}

void FontFaceSetDocument::FontLoadHistogram::UpdateStatus(FontFace* font_face) {
  if (status_ == kReported)
    return;
  if (font_face->HadBlankText())
    status_ = kHadBlankText;
  else if (status_ == kNoWebFonts)
    status_ = kDidNotHaveBlankText;
}

void FontFaceSetDocument::FontLoadHistogram::Record() {
  if (status_ == kHadBlankText || status_ == kDidNotHaveBlankText) {
    DEFINE_STATIC_LOCAL(EnumerationHistogram, had_blank_text_histogram,
                        ("WebFont.HadBlankText", 2));
    had_blank_text_histogram.Count(status_ == kHadBlankText ? 1 : 0);
    status_ = kReported;
  }
}

}  // namespace blink
