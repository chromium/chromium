// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/font_face_set.h"

#include "third_party/blink/renderer/core/css/font_face_cache.h"
#include "third_party/blink/renderer/core/css/font_face_set_load_event.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

const int FontFaceSet::kDefaultFontSize = 10;
const char FontFaceSet::kDefaultFontFamily[] = "sans-serif";

void FontFaceSet::HandlePendingEventsAndPromisesSoon() {
  if (!pending_task_queued_) {
    if (auto* context = GetExecutionContext()) {
      pending_task_queued_ = true;
      context->GetTaskRunner(TaskType::kFontLoading)
          ->PostTask(FROM_HERE,
                     WTF::Bind(&FontFaceSet::HandlePendingEventsAndPromises,
                               WrapPersistent(this)));
    }
  }
}

void FontFaceSet::HandlePendingEventsAndPromises() {
  pending_task_queued_ = false;
  if (!GetExecutionContext())
    return;
  FireLoadingEvent();
  FireDoneEventIfPossible();
}

void FontFaceSet::FireLoadingEvent() {
  if (should_fire_loading_event_) {
    should_fire_loading_event_ = false;
    DispatchEvent(
        *FontFaceSetLoadEvent::CreateForFontFaces(event_type_names::kLoading));
  }
}

FontFaceSet* FontFaceSet::addForBinding(ScriptState*,
                                        FontFace* font_face,
                                        ExceptionState&) {
  DCHECK(font_face);
  if (!InActiveContext())
    return this;
  if (non_css_connected_faces_.Contains(font_face))
    return this;
  if (IsCSSConnectedFontFace(font_face))
    return this;
  FontSelector* font_selector = GetFontSelector();
  non_css_connected_faces_.insert(font_face);
  font_selector->GetFontFaceCache()->AddFontFace(font_face, false);
  if (font_face->LoadStatus() == FontFace::kLoading)
    AddToLoadingFonts(font_face);
  font_selector->FontFaceInvalidated();
  return this;
}

void FontFaceSet::clearForBinding(ScriptState*, ExceptionState&) {
  if (!InActiveContext() || non_css_connected_faces_.IsEmpty())
    return;
  FontSelector* font_selector = GetFontSelector();
  FontFaceCache* font_face_cache = font_selector->GetFontFaceCache();
  for (const auto& font_face : non_css_connected_faces_) {
    font_face_cache->RemoveFontFace(font_face.Get(), false);
    if (font_face->LoadStatus() == FontFace::kLoading)
      RemoveFromLoadingFonts(font_face);
  }
  non_css_connected_faces_.clear();
  font_selector->FontFaceInvalidated();
}

bool FontFaceSet::deleteForBinding(ScriptState*,
                                   FontFace* font_face,
                                   ExceptionState&) {
  DCHECK(font_face);
  if (!InActiveContext())
    return false;
  HeapLinkedHashSet<Member<FontFace>>::iterator it =
      non_css_connected_faces_.find(font_face);
  if (it != non_css_connected_faces_.end()) {
    non_css_connected_faces_.erase(it);
    FontSelector* font_selector = GetFontSelector();
    font_selector->GetFontFaceCache()->RemoveFontFace(font_face, false);
    if (font_face->LoadStatus() == FontFace::kLoading)
      RemoveFromLoadingFonts(font_face);
    font_selector->FontFaceInvalidated();
    return true;
  }
  return false;
}

bool FontFaceSet::hasForBinding(ScriptState*,
                                FontFace* font_face,
                                ExceptionState&) const {
  DCHECK(font_face);
  if (!InActiveContext())
    return false;
  return non_css_connected_faces_.Contains(font_face) ||
         IsCSSConnectedFontFace(font_face);
}

void FontFaceSet::Trace(blink::Visitor* visitor) {
  visitor->Trace(non_css_connected_faces_);
  visitor->Trace(loading_fonts_);
  visitor->Trace(loaded_fonts_);
  visitor->Trace(failed_fonts_);
  visitor->Trace(ready_);
  ContextClient::Trace(visitor);
  EventTargetWithInlineData::Trace(visitor);
  FontFace::LoadFontCallback::Trace(visitor);
}

wtf_size_t FontFaceSet::size() const {
  if (!InActiveContext())
    return non_css_connected_faces_.size();
  return CSSConnectedFontFaceList().size() + non_css_connected_faces_.size();
}

void FontFaceSet::AddFontFacesToFontFaceCache(FontFaceCache* font_face_cache) {
  for (const auto& font_face : non_css_connected_faces_)
    font_face_cache->AddFontFace(font_face, false);
}

void FontFaceSet::AddToLoadingFonts(FontFace* font_face) {
  if (!is_loading_) {
    is_loading_ = true;
    should_fire_loading_event_ = true;
    if (ready_->GetState() != ReadyProperty::kPending)
      ready_->Reset();
    HandlePendingEventsAndPromisesSoon();
  }
  loading_fonts_.insert(font_face);
  font_face->AddCallback(this);
}

void FontFaceSet::RemoveFromLoadingFonts(FontFace* font_face) {
  loading_fonts_.erase(font_face);
  if (loading_fonts_.IsEmpty())
    HandlePendingEventsAndPromisesSoon();
}

void FontFaceSet::LoadFontPromiseResolver::LoadFonts() {
  if (!num_loading_) {
    resolver_->Resolve(font_faces_);
    return;
  }

  for (wtf_size_t i = 0; i < font_faces_.size(); i++)
    font_faces_[i]->LoadWithCallback(this);
}

ScriptPromise FontFaceSet::load(ScriptState* script_state,
                                const String& font_string,
                                const String& text) {
  if (!InActiveContext())
    return ScriptPromise();

  Font font;
  if (!ResolveFontStyle(font_string, font)) {
    auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
    ScriptPromise promise = resolver->Promise();
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kSyntaxError,
        "Could not resolve '" + font_string + "' as a font."));
    return promise;
  }

  FontFaceCache* font_face_cache = GetFontSelector()->GetFontFaceCache();
  FontFaceArray faces;
  for (const FontFamily* f = &font.GetFontDescription().Family(); f;
       f = f->Next()) {
    CSSSegmentedFontFace* segmented_font_face =
        font_face_cache->Get(font.GetFontDescription(), f->Family());
    if (segmented_font_face)
      segmented_font_face->Match(text, faces);
  }

  auto* resolver =
      MakeGarbageCollected<LoadFontPromiseResolver>(faces, script_state);
  ScriptPromise promise = resolver->Promise();
  // After this, resolver->promise() may return null.
  resolver->LoadFonts();
  return promise;
}

bool FontFaceSet::check(const String& font_string,
                        const String& text,
                        ExceptionState& exception_state) {
  if (!InActiveContext())
    return false;

  Font font;
  if (!ResolveFontStyle(font_string, font)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "Could not resolve '" + font_string + "' as a font.");
    return false;
  }

  FontSelector* font_selector = GetFontSelector();
  FontFaceCache* font_face_cache = font_selector->GetFontFaceCache();

  bool has_loaded_faces = false;
  for (const FontFamily* f = &font.GetFontDescription().Family(); f;
       f = f->Next()) {
    CSSSegmentedFontFace* face =
        font_face_cache->Get(font.GetFontDescription(), f->Family());
    if (face) {
      if (!face->CheckFont(text))
        return false;
      has_loaded_faces = true;
    }
  }
  if (has_loaded_faces)
    return true;
  for (const FontFamily* f = &font.GetFontDescription().Family(); f;
       f = f->Next()) {
    if (font_selector->IsPlatformFamilyMatchAvailable(font.GetFontDescription(),
                                                      f->Family()))
      return true;
  }
  return false;
}

void FontFaceSet::FireDoneEvent() {
  if (is_loading_) {
    FontFaceSetLoadEvent* done_event = nullptr;
    FontFaceSetLoadEvent* error_event = nullptr;
    done_event = FontFaceSetLoadEvent::CreateForFontFaces(
        event_type_names::kLoadingdone, loaded_fonts_);
    loaded_fonts_.clear();
    if (!failed_fonts_.IsEmpty()) {
      error_event = FontFaceSetLoadEvent::CreateForFontFaces(
          event_type_names::kLoadingerror, failed_fonts_);
      failed_fonts_.clear();
    }
    is_loading_ = false;
    DispatchEvent(*done_event);
    if (error_event)
      DispatchEvent(*error_event);
  }

  if (ready_->GetState() == ReadyProperty::kPending)
    ready_->Resolve(this);
}

bool FontFaceSet::ShouldSignalReady() const {
  if (!loading_fonts_.IsEmpty())
    return false;
  return is_loading_ || ready_->GetState() == ReadyProperty::kPending;
}

void FontFaceSet::LoadFontPromiseResolver::NotifyLoaded(FontFace* font_face) {
  num_loading_--;
  if (num_loading_ || error_occured_)
    return;

  resolver_->Resolve(font_faces_);
}

void FontFaceSet::LoadFontPromiseResolver::NotifyError(FontFace* font_face) {
  num_loading_--;
  if (!error_occured_) {
    error_occured_ = true;
    resolver_->Reject(font_face->GetError());
  }
}

void FontFaceSet::LoadFontPromiseResolver::Trace(blink::Visitor* visitor) {
  visitor->Trace(font_faces_);
  visitor->Trace(resolver_);
  LoadFontCallback::Trace(visitor);
}

bool FontFaceSet::IterationSource::Next(ScriptState*,
                                        Member<FontFace>& key,
                                        Member<FontFace>& value,
                                        ExceptionState&) {
  if (font_faces_.size() <= index_)
    return false;
  key = value = font_faces_[index_++];
  return true;
}

FontFaceSetIterable::IterationSource* FontFaceSet::StartIteration(
    ScriptState*,
    ExceptionState&) {
  // Setlike should iterate each item in insertion order, and items should
  // be keep on up to date. But since blink does not have a way to hook up CSS
  // modification, take a snapshot here, and make it ordered as follows.
  HeapVector<Member<FontFace>> font_faces;
  if (InActiveContext()) {
    const HeapLinkedHashSet<Member<FontFace>>& css_connected_faces =
        CSSConnectedFontFaceList();
    font_faces.ReserveInitialCapacity(css_connected_faces.size() +
                                      non_css_connected_faces_.size());
    for (const auto& font_face : css_connected_faces)
      font_faces.push_back(font_face);
    for (const auto& font_face : non_css_connected_faces_)
      font_faces.push_back(font_face);
  }
  return MakeGarbageCollected<IterationSource>(font_faces);
}

}  // namespace blink
