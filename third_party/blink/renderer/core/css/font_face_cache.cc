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
  if (!style_rule_to_font_face_.insert(font_face_rule, font_face).is_new_entry)
    return;
  AddFontFace(font_face, true);
}

void FontFaceCache::AddFontFace(FontFace* font_face, bool css_connected) {
  SegmentedFacesByFamily::AddResult capabilities_result =
      segmented_faces_.insert(font_face->family(), nullptr);

  if (capabilities_result.is_new_entry) {
    capabilities_result.stored_value->value =
        MakeGarbageCollected<CapabilitiesSet>();
  }

  DCHECK(font_face->GetFontSelectionCapabilities().IsValid() &&
         !font_face->GetFontSelectionCapabilities().IsHashTableDeletedValue());

  CapabilitiesSet::AddResult segmented_font_face_result =
      capabilities_result.stored_value->value->insert(
          font_face->GetFontSelectionCapabilities(), nullptr);
  if (segmented_font_face_result.is_new_entry) {
    segmented_font_face_result.stored_value->value =
        MakeGarbageCollected<CSSSegmentedFontFace>(
            font_face->GetFontSelectionCapabilities());
  }

  segmented_font_face_result.stored_value->value->AddFontFace(font_face,
                                                              css_connected);

  if (css_connected)
    css_connected_font_faces_.insert(font_face);

  font_selection_query_cache_.erase(font_face->family());
  IncrementVersion();
}

void FontFaceCache::Remove(const StyleRuleFontFace* font_face_rule) {
  StyleRuleToFontFace::iterator it =
      style_rule_to_font_face_.find(font_face_rule);
  if (it != style_rule_to_font_face_.end()) {
    RemoveFontFace(it->value.Get(), true);
    style_rule_to_font_face_.erase(it);
  }
}

void FontFaceCache::RemoveFontFace(FontFace* font_face, bool css_connected) {
  SegmentedFacesByFamily::iterator segmented_faces_iter =
      segmented_faces_.find(font_face->family());
  if (segmented_faces_iter == segmented_faces_.end())
    return;

  CapabilitiesSet* family_segmented_faces = segmented_faces_iter->value.Get();

  CapabilitiesSet::iterator family_segmented_faces_iter =
      family_segmented_faces->find(font_face->GetFontSelectionCapabilities());
  if (family_segmented_faces_iter == family_segmented_faces->end())
    return;

  CSSSegmentedFontFace* segmented_font_face =
      family_segmented_faces_iter->value;
  segmented_font_face->RemoveFontFace(font_face);
  if (segmented_font_face->IsEmpty()) {
    family_segmented_faces->erase(family_segmented_faces_iter);
    if (family_segmented_faces->IsEmpty()) {
      segmented_faces_.erase(segmented_faces_iter);
    }
  }

  font_selection_query_cache_.erase(font_face->family());

  if (css_connected)
    css_connected_font_faces_.erase(font_face);

  IncrementVersion();
}

bool FontFaceCache::ClearCSSConnected() {
  if (style_rule_to_font_face_.IsEmpty())
    return false;
  for (const auto& item : style_rule_to_font_face_)
    RemoveFontFace(item.value.Get(), true);
  style_rule_to_font_face_.clear();
  return true;
}

void FontFaceCache::ClearAll() {
  if (segmented_faces_.IsEmpty())
    return;

  segmented_faces_.clear();
  font_selection_query_cache_.clear();
  style_rule_to_font_face_.clear();
  css_connected_font_faces_.clear();
  IncrementVersion();
}

void FontFaceCache::IncrementVersion() {
  // Versions are guaranteed to be monotonically increasing, but not necessary
  // sequential within a thread.
  static base::AtomicSequenceNumber g_version;
  version_ = g_version.GetNext();
}

CSSSegmentedFontFace* FontFaceCache::Get(
    const FontDescription& font_description,
    const AtomicString& family) {
  if (family.IsEmpty())
    return nullptr;

  SegmentedFacesByFamily::iterator segmented_faces_for_family =
      segmented_faces_.find(family);
  if (segmented_faces_for_family == segmented_faces_.end() ||
      segmented_faces_for_family->value->IsEmpty())
    return nullptr;

  // TODO(crbug.com/1021568): Prevent `system-ui` from matching. Per spec,
  // generic family names should not match web fonts unless they are quoted.
  if (family == font_family_names::kSystemUi)
    return nullptr;

  auto family_faces = segmented_faces_for_family->value;

  // Either add or retrieve a cache entry in the selection query cache for the
  // specified family.
  FontSelectionQueryCache::AddResult cache_entry_for_family_add =
      font_selection_query_cache_.insert(
          family, MakeGarbageCollected<FontSelectionQueryResult>());
  auto cache_entry_for_family = cache_entry_for_family_add.stored_value->value;

  const FontSelectionRequest& request =
      font_description.GetFontSelectionRequest();

  FontSelectionQueryResult::AddResult face_entry =
      cache_entry_for_family->insert(request, nullptr);
  if (!face_entry.is_new_entry)
    return face_entry.stored_value->value;

  // If we don't have a previously cached result for this request, we now need
  // to iterate over all entries in the CapabilitiesSet for one family and
  // extract the best CSSSegmentedFontFace from those.

  // The FontSelectionAlgorithm needs to know the boundaries of stretch, style,
  // range for all the available faces in order to calculate distances
  // correctly.
  FontSelectionCapabilities all_faces_boundaries;
  for (const auto& item : *family_faces) {
    all_faces_boundaries.Expand(item.value->GetFontSelectionCapabilities());
  }

  FontSelectionAlgorithm font_selection_algorithm(request,
                                                  all_faces_boundaries);
  for (const auto& item : *family_faces) {
    const FontSelectionCapabilities& candidate_key = item.key;
    CSSSegmentedFontFace* candidate_value = item.value;
    if (!face_entry.stored_value->value ||
        font_selection_algorithm.IsBetterMatchForRequest(
            candidate_key,
            face_entry.stored_value->value->GetFontSelectionCapabilities())) {
      face_entry.stored_value->value = candidate_value;
    }
  }
  return face_entry.stored_value->value;
}

size_t FontFaceCache::GetNumSegmentedFacesForTesting() {
  size_t count = 0;
  for (auto& family_faces : segmented_faces_) {
    count += family_faces.value->size();
  }
  return count;
}

void FontFaceCache::Trace(blink::Visitor* visitor) {
  visitor->Trace(segmented_faces_);
  visitor->Trace(font_selection_query_cache_);
  visitor->Trace(style_rule_to_font_face_);
  visitor->Trace(css_connected_font_faces_);
}

}  // namespace blink
