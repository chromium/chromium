// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/font_face_set_worker.h"

#include "third_party/blink/renderer/bindings/core/v8/dictionary.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/css_segmented_font_face.h"
#include "third_party/blink/renderer/core/css/font_face_cache.h"
#include "third_party/blink/renderer/core/css/font_face_set_load_event.h"
#include "third_party/blink/renderer/core/css/offscreen_font_selector.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/resolver/font_style_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

// static
const char FontFaceSetWorker::kSupplementName[] = "FontFaceSetWorker";

FontFaceSetWorker::FontFaceSetWorker(WorkerGlobalScope& worker)
    : FontFaceSet(worker), Supplement<WorkerGlobalScope>(worker) {
}

FontFaceSetWorker::~FontFaceSetWorker() = default;

WorkerGlobalScope* FontFaceSetWorker::GetWorker() const {
  return To<WorkerGlobalScope>(GetExecutionContext());
}

AtomicString FontFaceSetWorker::status() const {
  DEFINE_STATIC_LOCAL(AtomicString, loading, ("loading"));
  DEFINE_STATIC_LOCAL(AtomicString, loaded, ("loaded"));
  return is_loading_ ? loading : loaded;
}

void FontFaceSetWorker::BeginFontLoading(FontFace* font_face) {
  AddToLoadingFonts(font_face);
}

void FontFaceSetWorker::NotifyLoaded(FontFace* font_face) {
  loaded_fonts_.push_back(font_face);
  RemoveFromLoadingFonts(font_face);
}

void FontFaceSetWorker::NotifyError(FontFace* font_face) {
  failed_fonts_.push_back(font_face);
  RemoveFromLoadingFonts(font_face);
}

ScriptPromise FontFaceSetWorker::ready(ScriptState* script_state) {
  return ready_->Promise(script_state->World());
}

void FontFaceSetWorker::FireDoneEventIfPossible() {
  if (should_fire_loading_event_)
    return;
  if (!ShouldSignalReady())
    return;

  FireDoneEvent();
}

bool FontFaceSetWorker::ResolveFontStyle(const String& font_string,
                                         Font& font) {
  if (font_string.IsEmpty())
    return false;

  // Interpret fontString in the same way as the 'font' attribute of
  // CanvasRenderingContext2D.
  auto* parsed_style =
      MakeGarbageCollected<MutableCSSPropertyValueSet>(kHTMLStandardMode);
  CSSParser::ParseValue(parsed_style, CSSPropertyID::kFont, font_string, true,
                        GetExecutionContext()->GetSecureContextMode());
  if (parsed_style->IsEmpty())
    return false;

  String font_value = parsed_style->GetPropertyValue(CSSPropertyID::kFont);
  if (font_value == "inherit" || font_value == "initial")
    return false;

  FontFamily font_family;
  font_family.SetFamily(FontFaceSet::kDefaultFontFamily);

  FontDescription default_font_description;
  default_font_description.SetFamily(font_family);
  default_font_description.SetSpecifiedSize(FontFaceSet::kDefaultFontSize);
  default_font_description.SetComputedSize(FontFaceSet::kDefaultFontSize);

  FontDescription description = FontStyleResolver::ComputeFont(
      *parsed_style, GetWorker()->GetFontSelector());

  font = Font(description);
  font.Update(GetWorker()->GetFontSelector());

  return true;
}

FontFaceSetWorker* FontFaceSetWorker::From(WorkerGlobalScope& worker) {
  FontFaceSetWorker* fonts =
      Supplement<WorkerGlobalScope>::From<FontFaceSetWorker>(worker);
  if (!fonts) {
    fonts = MakeGarbageCollected<FontFaceSetWorker>(worker);
    ProvideTo(worker, fonts);
  }

  return fonts;
}

void FontFaceSetWorker::Trace(Visitor* visitor) {
  Supplement<WorkerGlobalScope>::Trace(visitor);
  FontFaceSet::Trace(visitor);
}

}  // namespace blink
