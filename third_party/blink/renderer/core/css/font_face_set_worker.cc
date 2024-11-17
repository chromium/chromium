// Copyright 2017 The Chromium Authors
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
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/css/resolver/font_style_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

// static
const char FontFaceSetWorker::kSupplementName[] = "FontFaceSetWorker";

FontFaceSetWorker::FontFaceSetWorker(WorkerGlobalScope& worker)
    : FontFaceSet(worker), Supplement<WorkerGlobalScope>(worker) {}

FontFaceSetWorker::~FontFaceSetWorker() = default;

WorkerGlobalScope* FontFaceSetWorker::GetWorker() const {
  return To<WorkerGlobalScope>(GetExecutionContext());
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

ScriptPromise<FontFaceSet> FontFaceSetWorker::ready(ScriptState* script_state) {
  return ready_->Promise(script_state->World());
}

void FontFaceSetWorker::FireDoneEventIfPossible() {
  if (should_fire_loading_event_) {
    return;
  }
  if (!ShouldSignalReady()) {
    return;
  }

  FireDoneEvent();
}

bool FontFaceSetWorker::ResolveFontStyle(const String& font_string,
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

  FontDescription default_font_description;
  default_font_description.SetFamily(FontFamily(
      FontFaceSet::DefaultFontFamily(),
      FontFamily::InferredTypeFor(FontFaceSet::DefaultFontFamily())));
  default_font_description.SetSpecifiedSize(FontFaceSet::kDefaultFontSize);
  default_font_description.SetComputedSize(FontFaceSet::kDefaultFontSize);

  FontDescription description = FontStyleResolver::ComputeFont(
      *parsed_style, GetWorker()->GetFontSelector());

  font = Font(description, GetWorker()->GetFontSelector());

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

void FontFaceSetWorker::Trace(Visitor* visitor) const {
  Supplement<WorkerGlobalScope>::Trace(visitor);
  FontFaceSet::Trace(visitor);
}

}  // namespace blink
