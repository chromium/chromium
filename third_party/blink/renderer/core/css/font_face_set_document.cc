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

#include "base/metrics/histogram_functions.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/bindings/core/v8/dictionary.h"
#include "third_party/blink/renderer/core/css/css_font_face.h"
#include "third_party/blink/renderer/core/css/css_font_selector.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/css_segmented_font_face.h"
#include "third_party/blink/renderer/core/css/font_face_cache.h"
#include "third_party/blink/renderer/core/css/font_face_set_load_event.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/css/resolver/font_style_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

// static
const char FontFaceSetDocument::kSupplementName[] = "FontFaceSetDocument";

FontFaceSetDocument::FontFaceSetDocument(Document& document)
    : FontFaceSet(*document.GetExecutionContext()),
      Supplement<Document>(document),
      lcp_limit_timer_(document.GetTaskRunner(TaskType::kInternalLoading),
                       this,
                       &FontFaceSetDocument::LCPLimitReached) {}

FontFaceSetDocument::~FontFaceSetDocument() = default;

bool FontFaceSetDocument::InActiveContext() const {
  ExecutionContext* context = GetExecutionContext();
  return context && To<LocalDOMWindow>(context)->document()->IsActive();
}

FontSelector* FontFaceSetDocument::GetFontSelector() const {
  DCHECK(IsMainThread());
  return GetDocument()->GetStyleEngine().GetFontSelector();
}

void FontFaceSetDocument::DidLayout() {
  if (!GetExecutionContext()) {
    return;
  }
  if (GetDocument()->IsInOutermostMainFrame() && loading_fonts_.empty()) {
    font_load_histogram_.Record();
  }
  if (!ShouldSignalReady()) {
    return;
  }
  HandlePendingEventsAndPromisesSoon();
}

void FontFaceSetDocument::StartLCPLimitTimerIfNeeded() {
  // Make sure the timer is started at most once for each document, and only
  // when the feature is enabled
  if (!base::FeatureList::IsEnabled(
          features::kAlignFontDisplayAutoTimeoutWithLCPGoal) ||
      has_reached_lcp_limit_ || lcp_limit_timer_.IsActive() ||
      !GetDocument()->Loader()) {
    return;
  }

  lcp_limit_timer_.StartOneShot(
      GetDocument()->Loader()->RemainingTimeToLCPLimit(), FROM_HERE);
}

void FontFaceSetDocument::BeginFontLoading(FontFace* font_face) {
  AddToLoadingFonts(font_face);
  StartLCPLimitTimerIfNeeded();
}

void FontFaceSetDocument::NotifyLoaded(FontFace* font_face) {
  font_load_histogram_.UpdateStatus(font_face);
  loaded_fonts_.push_back(font_face);
  RemoveFromLoadingFonts(font_face);
}

void FontFaceSetDocument::NotifyError(FontFace* font_face) {
  font_load_histogram_.UpdateStatus(font_face);
  failed_fonts_.push_back(font_face);
  RemoveFromLoadingFonts(font_face);
}

size_t FontFaceSetDocument::ApproximateBlankCharacterCount() const {
  size_t count = 0;
  for (auto& font_face : loading_fonts_) {
    count += font_face->ApproximateBlankCharacterCount();
  }
  return count;
}

ScriptPromise<FontFaceSet> FontFaceSetDocument::ready(
    ScriptState* script_state) {
  if (ready_->GetState() != ReadyProperty::kPending && InActiveContext()) {
    // |ready_| is already resolved, but there may be pending stylesheet
    // changes and/or layout operations that may cause another font loads.
    // So synchronously update style and layout here.
    // This may trigger font loads, and replace |ready_| with a new Promise.
    GetDocument()->UpdateStyleAndLayout(DocumentUpdateReason::kJavaScript);
  }
  return ready_->Promise(script_state->World());
}

const HeapLinkedHashSet<Member<FontFace>>&
FontFaceSetDocument::CSSConnectedFontFaceList() const {
  Document* document = GetDocument();
  document->GetStyleEngine().UpdateActiveStyle();
  return GetFontSelector()->GetFontFaceCache()->CssConnectedFontFaces();
}

void FontFaceSetDocument::FireDoneEventIfPossible() {
  if (should_fire_loading_event_) {
    return;
  }
  if (!ShouldSignalReady()) {
    return;
  }
  Document* d = GetDocument();
  if (!d) {
    return;
  }

  // If the layout was invalidated in between when we thought layout
  // was updated and when we're ready to fire the event, just wait
  // until after the next layout before firing events.
  if (!d->View() || d->View()->NeedsLayout()) {
    return;
  }

  FireDoneEvent();
}

bool FontFaceSetDocument::ResolveFontStyle(const String& font_string,
                                           Font& font) {
  if (font_string.empty()) {
    return false;
  }

  // Interpret fontString in the same way as the 'font' attribute of
  // CanvasRenderingContext2D.
  auto* parsed_style = CSSParser::ParseFont(font_string, GetExecutionContext());
  if (!parsed_style) {
    return false;
  }

  if (!GetDocument()->documentElement()) {
    auto* font_selector = GetDocument()->GetStyleEngine().GetFontSelector();
    FontDescription description =
        FontStyleResolver::ComputeFont(*parsed_style, font_selector);
    font = Font(description, font_selector);
    return true;
  }

  ComputedStyleBuilder builder =
      GetDocument()->GetStyleResolver().CreateComputedStyleBuilder();

  FontDescription default_font_description;
  default_font_description.SetFamily(FontFamily(
      FontFaceSet::DefaultFontFamily(),
      FontFamily::InferredTypeFor(FontFaceSet::DefaultFontFamily())));
  default_font_description.SetSpecifiedSize(FontFaceSet::kDefaultFontSize);
  default_font_description.SetComputedSize(FontFaceSet::kDefaultFontSize);

  builder.SetFontDescription(default_font_description);
  const ComputedStyle* style = builder.TakeStyle();

  font = GetDocument()->GetStyleEngine().ComputeFont(
      *GetDocument()->documentElement(), *style, *parsed_style);

  // StyleResolver::ComputeFont() should have set the document's FontSelector
  // to |style|.
  DCHECK_EQ(font.GetFontSelector(), GetFontSelector());

  return true;
}

Document* FontFaceSetDocument::GetDocument() const {
  if (auto* window = To<LocalDOMWindow>(GetExecutionContext())) {
    return window->document();
  }
  return nullptr;
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
  if (!document.LoadEventFinished()) {
    // https://www.w3.org/TR/2014/WD-css-font-loading-3-20140522/#font-face-set-ready
    // doesn't say when document.fonts.ready should actually fire, but the
    // existing tests depend on it firing after onload.
    return;
  }
  if (FontFaceSetDocument* fonts =
          Supplement<Document>::From<FontFaceSetDocument>(document)) {
    fonts->DidLayout();
  }
}

size_t FontFaceSetDocument::ApproximateBlankCharacterCount(Document& document) {
  if (FontFaceSetDocument* fonts =
          Supplement<Document>::From<FontFaceSetDocument>(document)) {
    return fonts->ApproximateBlankCharacterCount();
  }
  return 0;
}

void FontFaceSetDocument::AlignTimeoutWithLCPGoal(FontFace* font_face) {
  font_face->CssFontFace()->UpdatePeriod();
}

void FontFaceSetDocument::LCPLimitReached(TimerBase*) {
  DCHECK(base::FeatureList::IsEnabled(
      features::kAlignFontDisplayAutoTimeoutWithLCPGoal));
  if (!GetDocument() || !GetDocument()->IsActive()) {
    return;
  }
  has_reached_lcp_limit_ = true;
  for (FontFace* font_face : CSSConnectedFontFaceList()) {
    AlignTimeoutWithLCPGoal(font_face);
  }
  for (FontFace* font_face : non_css_connected_faces_) {
    AlignTimeoutWithLCPGoal(font_face);
  }
}

void FontFaceSetDocument::Trace(Visitor* visitor) const {
  visitor->Trace(lcp_limit_timer_);
  Supplement<Document>::Trace(visitor);
  FontFaceSet::Trace(visitor);
}

void FontFaceSetDocument::FontLoadHistogram::UpdateStatus(FontFace* font_face) {
  if (status_ == kReported) {
    return;
  }
  if (font_face->HadBlankText()) {
    status_ = kHadBlankText;
  } else if (status_ == kNoWebFonts) {
    status_ = kDidNotHaveBlankText;
  }
}

void FontFaceSetDocument::FontLoadHistogram::Record() {
  if (status_ == kHadBlankText || status_ == kDidNotHaveBlankText) {
    base::UmaHistogramBoolean("WebFont.HadBlankText", status_ == kHadBlankText);
    status_ = kReported;
  }
}

}  // namespace blink
