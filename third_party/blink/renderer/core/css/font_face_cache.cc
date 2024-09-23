/*
 * Copyright (C) 2007, 2008, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/css/font_face_cache.h"

#include <numeric>
#include "base/atomic_sequence_num.h"
#include "third_party/blink/renderer/core/css/css_segmented_font_face.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/loader/resource/font_resource.h"
#include "third_party/blink/renderer/platform/font_family_names.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/fonts/font_selection_algorithm.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

FontFaceCache::FontFaceCache() : version_(0) {}

void FontFaceCache::Add(const StyleRuleFontFace* font_face_rule,
                        FontFace* font_face) {
  if (!style_rule_to_font_face_.insert(font_face_rule, font_face)
           .is_new_entry) {
    return;
  }
  AddFontFace(font_face, true);
}

void FontFaceCache::SegmentedFacesByFamily::AddFontFace(FontFace* font_face,
                                                        bool css_connected) {
  const auto result = map_.insert(font_face->family(), nullptr);
  if (result.is_new_entry) {
    result.stored_value->value = MakeGarbageCollected<CapabilitiesSet>();
  }

  CapabilitiesSet* family_faces = result.stored_value->value;
  family_faces->AddFontFace(font_face, css_connected);
}

void FontFaceCache::AddFontFace(FontFace* font_face, bool css_connected) {
  DCHECK(font_face->GetFontSelectionCapabilities().IsValid() &&
         !font_face->GetFontSelectionCapabilities().IsHashTableDeletedValue());

  segmented_faces_.AddFontFace(font_face, css_connected);

  if (css_connected) {
    css_connected_font_faces_.insert(font_face);
  }

  font_selection_query_cache_.Remove(font_face->family());
  IncrementVersion();
}

void FontFaceCache::FontSelectionQueryCache::Remove(
    const AtomicString& family) {
  map_.erase(family);
}

void FontFaceCache::CapabilitiesSet::AddFontFace(FontFace* font_face,
                                                 bool css_connected) {
  const auto result =
      map_.insert(font_face->GetFontSelectionCapabilities(), nullptr);
  if (result.is_new_entry) {
    result.stored_value->value = MakeGarbageCollected<CSSSegmentedFontFace>(
        font_face->GetFontSelectionCapabilities());
  }

  result.stored_value->value->AddFontFace(font_face, css_connected);
}

void FontFaceCache::Remove(const StyleRuleFontFace* font_face_rule) {
  StyleRuleToFontFace::iterator it =
      style_rule_to_font_face_.find(font_face_rule);
  if (it != style_rule_to_font_face_.end()) {
    RemoveFontFace(it->value.Get(), true);
    style_rule_to_font_face_.erase(it);
  }
}

bool FontFaceCache::SegmentedFacesByFamily::RemoveFontFace(
    FontFace* font_face) {
  const auto it = map_.find(font_face->family());
  if (it == map_.end()) {
    return false;
  }

  CapabilitiesSet* family_segmented_faces = it->value;
  if (family_segmented_faces->RemoveFontFace(font_face)) {
    map_.erase(it);
  }
  return true;
}

void FontFaceCache::RemoveFontFace(FontFace* font_face, bool css_connected) {
  if (!segmented_faces_.RemoveFontFace(font_face)) {
    return;
  }

  font_selection_query_cache_.Remove(font_face->family());

  if (css_connected) {
    css_connected_font_faces_.erase(font_face);
  }

  IncrementVersion();
}

bool FontFaceCache::CapabilitiesSet::RemoveFontFace(FontFace* font_face) {
  Map::iterator it = map_.find(font_face->GetFontSelectionCapabilities());
  if (it == map_.end()) {
    return false;
  }

  CSSSegmentedFontFace* segmented_font_face = it->value;
  segmented_font_face->RemoveFontFace(font_face);
  if (!segmented_font_face->IsEmpty()) {
    return false;
  }
  map_.erase(it);
  return map_.empty();
}

bool FontFaceCache::ClearCSSConnected() {
  if (style_rule_to_font_face_.empty()) {
    return false;
  }
  for (const auto& item : style_rule_to_font_face_) {
    RemoveFontFace(item.value.Get(), true);
  }
  style_rule_to_font_face_.clear();
  return true;
}

void FontFaceCache::ClearAll() {
  if (segmented_faces_.IsEmpty()) {
    return;
  }

  segmented_faces_.Clear();
  font_selection_query_cache_.Clear();
  style_rule_to_font_face_.clear();
  css_connected_font_faces_.clear();
  IncrementVersion();
}

void FontFaceCache::FontSelectionQueryCache::Clear() {
  map_.clear();
}

void FontFaceCache::IncrementVersion() {
  // Versions are guaranteed to be monotonically increasing, but not necessary
  // sequential within a thread.
  static base::AtomicSequenceNumber g_version;
  version_ = g_version.GetNext();
}

FontFaceCache::CapabilitiesSet* FontFaceCache::SegmentedFacesByFamily::Find(
    const AtomicString& family) const {
  const auto it = map_.find(family);
  if (it == map_.end()) {
    return nullptr;
  }
  return it->value.Get();
}

CSSSegmentedFontFace* FontFaceCache::Get(
    const FontDescription& font_description,
    const AtomicString& family) {
  CapabilitiesSet* family_faces = segmented_faces_.Find(family);
  if (!family_faces) {
    return nullptr;
  }

  return font_selection_query_cache_.GetOrCreate(
      font_description.GetFontSelectionRequest(), family, family_faces);
}

CSSSegmentedFontFace* FontFaceCache::FontSelectionQueryCache::GetOrCreate(
    const FontSelectionRequest& request,
    const AtomicString& family,
    CapabilitiesSet* family_faces) {
  const auto result = map_.insert(family, nullptr);
  if (result.is_new_entry) {
    result.stored_value->value =
        MakeGarbageCollected<FontSelectionQueryResult>();
  }
  return result.stored_value->value->GetOrCreate(request, *family_faces);
}

CSSSegmentedFontFace* FontFaceCache::FontSelectionQueryResult::GetOrCreate(
    const FontSelectionRequest& request,
    const CapabilitiesSet& family_faces) {
  const auto face_entry = map_.insert(request, nullptr);
  if (!face_entry.is_new_entry) {
    return face_entry.stored_value->value.Get();
  }

  // If we don't have a previously cached result for this request, we now need
  // to iterate over all entries in the CapabilitiesSet for one family and
  // extract the best CSSSegmentedFontFace from those.

  // The FontSelectionAlgorithm needs to know the boundaries of stretch, style,
  // range for all the available faces in order to calculate distances
  // correctly.
  FontSelectionCapabilities all_faces_boundaries;
  for (const auto& item : family_faces) {
    all_faces_boundaries.Expand(item.value->GetFontSelectionCapabilities());
  }

  FontSelectionAlgorithm font_selection_algorithm(request,
                                                  all_faces_boundaries);
  for (const auto& item : family_faces) {
    const FontSelectionCapabilities& candidate_key = item.key;
    CSSSegmentedFontFace* candidate_value = item.value;
    if (!face_entry.stored_value->value ||
        font_selection_algorithm.IsBetterMatchForRequest(
            candidate_key,
            face_entry.stored_value->value->GetFontSelectionCapabilities())) {
      face_entry.stored_value->value = candidate_value;
    }
  }
  return face_entry.stored_value->value.Get();
}

size_t FontFaceCache::GetNumSegmentedFacesForTesting() {
  return segmented_faces_.GetNumSegmentedFacesForTesting();
}

size_t FontFaceCache::SegmentedFacesByFamily::GetNumSegmentedFacesForTesting()
    const {
  return std::accumulate(
      map_.begin(), map_.end(), 0,
      [](size_t sum, const auto& entry) { return sum + entry.value->size(); });
}

void FontFaceCache::Trace(Visitor* visitor) const {
  visitor->Trace(segmented_faces_);
  visitor->Trace(font_selection_query_cache_);
  visitor->Trace(style_rule_to_font_face_);
  visitor->Trace(css_connected_font_faces_);
}

void FontFaceCache::CapabilitiesSet::Trace(Visitor* visitor) const {
  visitor->Trace(map_);
}

void FontFaceCache::FontSelectionQueryCache::Trace(Visitor* visitor) const {
  visitor->Trace(map_);
}

void FontFaceCache::FontSelectionQueryResult::Trace(Visitor* visitor) const {
  visitor->Trace(map_);
}

void FontFaceCache::SegmentedFacesByFamily::Trace(Visitor* visitor) const {
  visitor->Trace(map_);
}

}  // namespace blink
