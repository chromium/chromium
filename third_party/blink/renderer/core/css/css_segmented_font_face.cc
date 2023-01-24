/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/css/css_segmented_font_face.h"

#include "base/debug/dump_without_crashing.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "third_party/blink/renderer/core/css/cascade_layer_map.h"
#include "third_party/blink/renderer/core/css/css_font_face.h"
#include "third_party/blink/renderer/core/css/css_font_selector.h"
#include "third_party/blink/renderer/core/css/resolver/scoped_style_resolver.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/fonts/font_face_creation_params.h"
#include "third_party/blink/renderer/platform/fonts/segmented_font_data.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"

// See comment below in CSSSegmentedFontFace::GetFontData - the cache from
// CSSSegmentedFontFace (which represents a group of @font-face declarations
// with identical FontSelectionCapabilities but differing by unicode-range) to
// FontData/SegmentedFontData, (i.e. the actual font blobs that can be used for
// shaping and painting retrieved from a CSSFontFaceSource) is usually small
// (less than a dozen, up to tens) for non-animation-cases, but grows fast to
// thousands when animating variable font parameters. Set a limit until we start
// dropping cache entries in animation scenarios.
static constexpr size_t kFontDataTableMaxSize = 250;

namespace blink {

// static
CSSSegmentedFontFace* CSSSegmentedFontFace::Create(
    FontSelectionCapabilities capabilities) {
  return MakeGarbageCollected<CSSSegmentedFontFace>(capabilities);
}

CSSSegmentedFontFace::CSSSegmentedFontFace(
    FontSelectionCapabilities font_selection_capabilities)
    : font_selection_capabilities_(font_selection_capabilities),
      font_data_table_(kFontDataTableMaxSize),
      font_faces_(MakeGarbageCollected<FontFaceList>()),
      approximate_character_count_(0) {}

CSSSegmentedFontFace::~CSSSegmentedFontFace() = default;

void CSSSegmentedFontFace::PruneTable() {
  // Make sure the glyph page tree prunes out all uses of this custom font.
  if (!font_data_table_.size()) {
    return;
  }

  font_data_table_.Clear();
}

bool CSSSegmentedFontFace::IsValid() const {
  // Valid if at least one font face is valid.
  return font_faces_->ForEachUntilTrue(
      WTF::BindRepeating([](Member<FontFace> font_face) -> bool {
        if (font_face->CssFontFace()->IsValid()) {
          return true;
        }
        return false;
      }));
}

void CSSSegmentedFontFace::FontFaceInvalidated() {
  PruneTable();
}

void CSSSegmentedFontFace::AddFontFace(FontFace* font_face,
                                       bool css_connected) {
  PruneTable();
  font_face->CssFontFace()->AddSegmentedFontFace(this);
  font_faces_->Insert(font_face, css_connected);
}

void CSSSegmentedFontFace::RemoveFontFace(FontFace* font_face) {
  if (!font_faces_->Erase(font_face)) {
    return;
  }

  PruneTable();
  font_face->CssFontFace()->RemoveSegmentedFontFace(this);
}

scoped_refptr<FontData> CSSSegmentedFontFace::GetFontData(
    const FontDescription& font_description) {
  if (!IsValid()) {
    return nullptr;
  }

  bool is_unique_match = false;
  bool is_generic_family = false;
  FontCacheKey key = font_description.CacheKey(
      FontFaceCreationParams(), is_unique_match, is_generic_family);

  // font_data_table_ caches FontData and SegmentedFontData instances, which
  // provide SimpleFontData objects containing FontPlatformData objects. In the
  // case of variable font animations, the variable instance SkTypeface is
  // contained in these FontPlatformData objects. In other words, this cache
  // stores the recently used variable font instances during a variable font
  // animation. The cache reflects in how many different sizes, synthetic styles
  // (bold / italic synthetic versions), or for variable fonts, in how many
  // variable instances (stretch/style/weightand font-variation-setings
  // variations) the font is instantiated. In non animation scenarios, there is
  // usually only a small number of FontData/SegmentedFontData instances created
  // per CSSSegmentedFontFace. Whereas in variable font animations, this number
  // grows rapidly.
  auto it = font_data_table_.Get(key);
  if (it != font_data_table_.end()) {
    scoped_refptr<SegmentedFontData> cached_font_data = it->second;
    if (cached_font_data && cached_font_data->NumFaces()) {
      return cached_font_data;
    }
  }

  scoped_refptr<SegmentedFontData> created_font_data =
      SegmentedFontData::Create();

  FontDescription requested_font_description(font_description);
  const FontSelectionRequest& font_selection_request =
      font_description.GetFontSelectionRequest();
  requested_font_description.SetSyntheticBold(
      font_selection_capabilities_.weight.maximum < BoldThreshold() &&
      font_selection_request.weight >= BoldThreshold() &&
      font_description.SyntheticBoldAllowed());
  requested_font_description.SetSyntheticItalic(
      font_selection_capabilities_.slope.maximum < ItalicSlopeValue() &&
      font_selection_request.slope >= ItalicSlopeValue() &&
      font_description.SyntheticItalicAllowed());

  font_faces_->ForEachReverse(WTF::BindRepeating(
      [](const FontDescription& requested_font_description,
         scoped_refptr<SegmentedFontData> created_font_data,
         Member<FontFace> font_face) {
        if (!font_face->CssFontFace()->IsValid()) {
          return;
        }
        if (scoped_refptr<SimpleFontData> face_font_data =
                font_face->CssFontFace()->GetFontData(
                    requested_font_description)) {
          DCHECK(!face_font_data->IsSegmented());
          if (face_font_data->IsCustomFont()) {
            created_font_data->AppendFace(base::AdoptRef(
                new FontDataForRangeSet(std::move(face_font_data),
                                        font_face->CssFontFace()->Ranges())));
          } else {
            created_font_data->AppendFace(
                base::AdoptRef(new FontDataForRangeSetFromCache(
                    std::move(face_font_data),
                    font_face->CssFontFace()->Ranges())));
          }
        }
      },
      requested_font_description, created_font_data));

  if (created_font_data->NumFaces()) {
    scoped_refptr<SegmentedFontData> put_to_cache(created_font_data);
    font_data_table_.Put(std::move(key), std::move(put_to_cache));
    // No release, we have a reference to an object in the cache which should
    // retain the ref count it has.
    return created_font_data;
  }

  return nullptr;
}

void CSSSegmentedFontFace::WillUseFontData(
    const FontDescription& font_description,
    const String& text) {
  approximate_character_count_ += text.length();
  font_faces_->ForEachReverseUntilTrue(WTF::BindRepeating(
      [](const FontDescription& font_description, const String& text,
         Member<FontFace> font_face) -> bool {
        if (font_face->LoadStatus() != FontFace::kUnloaded) {
          return true;
        }
        if (font_face->CssFontFace()->MaybeLoadFont(font_description, text)) {
          return true;
        }
        return false;
      },
      font_description, text));
}

void CSSSegmentedFontFace::WillUseRange(
    const blink::FontDescription& font_description,
    const blink::FontDataForRangeSet& range_set) {
  // Iterating backwards since later defined unicode-range faces override
  // previously defined ones, according to the CSS3 fonts module.
  // https://drafts.csswg.org/css-fonts/#composite-fonts
  font_faces_->ForEachReverseUntilTrue(WTF::BindRepeating(
      [](const blink::FontDescription& font_description,
         const blink::FontDataForRangeSet& range_set,
         Member<FontFace> font_face) -> bool {
        CSSFontFace* css_font_face = font_face->CssFontFace();
        if (css_font_face->MaybeLoadFont(font_description, range_set)) {
          return true;
        }
        return false;
      },
      font_description, range_set));
}

bool CSSSegmentedFontFace::CheckFont(const String& text) const {
  return font_faces_->ForEachUntilFalse(WTF::BindRepeating(
      [](const String& text, Member<FontFace> font_face) -> bool {
        if (font_face->LoadStatus() != FontFace::kLoaded &&
            font_face->CssFontFace()->Ranges()->IntersectsWith(text)) {
          return false;
        }
        return true;
      },
      text));
}

void CSSSegmentedFontFace::Match(const String& text,
                                 HeapVector<Member<FontFace>>* faces) const {
  // WTF::BindRepeating requires WrapPersistent around |faces|, which is fine,
  // because the wrap's lifetime is contained to this function.
  font_faces_->ForEach(WTF::BindRepeating(
      [](const String& text, HeapVector<Member<FontFace>>* faces,
         Member<FontFace> font_face) {
        if (font_face->CssFontFace()->Ranges()->IntersectsWith(text)) {
          faces->push_back(font_face);
        }
      },
      text, WrapPersistent(faces)));
}

void CSSSegmentedFontFace::Trace(Visitor* visitor) const {
  visitor->Trace(font_faces_);
}

bool FontFaceList::IsEmpty() const {
  return css_connected_face_.empty() && non_css_connected_face_.empty();
}

namespace {

bool CascadePriorityHigherThan(const FontFace& new_font_face,
                               const FontFace& existing_font_face) {
  // We should reach here only for CSS-connected font faces, which must have an
  // owner document. However, there are cases where we don't have a document
  // here, possibly caused by ExecutionContext or Document lifecycle issues.
  // TODO(crbug.com/1250831): Find out the root cause and fix it.
  if (!new_font_face.GetDocument() || !existing_font_face.GetDocument()) {
    base::debug::DumpWithoutCrashing();
    // In the buggy case, to ensure a stable ordering, font faces without a
    // document are considered higher priority.
    return !new_font_face.GetDocument();
  }
  DCHECK_EQ(new_font_face.GetDocument(), existing_font_face.GetDocument());
  DCHECK(new_font_face.GetStyleRule());
  DCHECK(existing_font_face.GetStyleRule());
  if (new_font_face.IsUserStyle() != existing_font_face.IsUserStyle()) {
    return existing_font_face.IsUserStyle();
  }
  const CascadeLayerMap* map = nullptr;
  if (new_font_face.IsUserStyle()) {
    map =
        new_font_face.GetDocument()->GetStyleEngine().GetUserCascadeLayerMap();
  } else if (new_font_face.GetDocument()->GetScopedStyleResolver()) {
    map = new_font_face.GetDocument()
              ->GetScopedStyleResolver()
              ->GetCascadeLayerMap();
  }
  if (!map) {
    return true;
  }
  return map->CompareLayerOrder(
             existing_font_face.GetStyleRule()->GetCascadeLayer(),
             new_font_face.GetStyleRule()->GetCascadeLayer()) <= 0;
}

}  // namespace

void FontFaceList::Insert(FontFace* font_face, bool css_connected) {
  if (!css_connected) {
    non_css_connected_face_.insert(font_face);
    return;
  }

  auto it = css_connected_face_.end();
  while (it != css_connected_face_.begin()) {
    auto prev = it;
    --prev;
    if (CascadePriorityHigherThan(*font_face, **prev)) {
      break;
    }
    it = prev;
  }

  css_connected_face_.InsertBefore(it, font_face);
}

bool FontFaceList::Erase(FontFace* font_face) {
  FontFaceListPart::iterator it = css_connected_face_.find(font_face);
  if (it != css_connected_face_.end()) {
    css_connected_face_.erase(it);
    return true;
  }
  it = non_css_connected_face_.find(font_face);
  if (it != non_css_connected_face_.end()) {
    non_css_connected_face_.erase(it);
    return true;
  }
  return false;
}

// A callback that will be fed into |ForEach*UntilTrue|.
// |callback| wants the |ForEach*UntilFalse| operation to stop iterating on
// false, so its return value has to be negated in order for it to work with
// |ForEach*UntilTrue|.
static bool NegatingCallback(
    const base::RepeatingCallback<bool(Member<FontFace>)>& callback,
    Member<FontFace> font_face) {
  return !callback.Run(font_face);
}

// A callback that will be fed into |ForEach*UntilTrue|, when we want it to
// always continue iterating, so false has to be always returned.
static bool FalseReturningCallback(
    const base::RepeatingCallback<void(Member<FontFace>)>& callback,
    Member<FontFace> font_face) {
  callback.Run(font_face);
  return false;
}

bool FontFaceList::ForEachUntilTrue(
    const base::RepeatingCallback<bool(Member<FontFace>)>& callback) const {
  for (Member<FontFace> font_face : css_connected_face_) {
    if (callback.Run(font_face)) {
      return true;
    }
  }
  for (Member<FontFace> font_face : non_css_connected_face_) {
    if (callback.Run(font_face)) {
      return true;
    }
  }
  return false;
}

bool FontFaceList::ForEachUntilFalse(
    const base::RepeatingCallback<bool(Member<FontFace>)>& callback) const {
  base::RepeatingCallback<bool(Member<FontFace>)> negating_callback =
      WTF::BindRepeating(NegatingCallback, callback);
  // When |callback| returns |false|, |ForEachUntilTrue(negating_callback)| will
  // stop iterating and return |true|, so negate it.
  return !ForEachUntilTrue(negating_callback);
}

void FontFaceList::ForEach(
    const base::RepeatingCallback<void(Member<FontFace>)>& func) const {
  base::RepeatingCallback<bool(Member<FontFace>)> false_returning_callback =
      WTF::BindRepeating(FalseReturningCallback, func);
  ForEachUntilTrue(false_returning_callback);
}

bool FontFaceList::ForEachReverseUntilTrue(
    const base::RepeatingCallback<bool(Member<FontFace>)>& func) const {
  for (auto it = non_css_connected_face_.rbegin();
       it != non_css_connected_face_.rend(); ++it) {
    if (func.Run(*it)) {
      return true;
    }
  }
  for (auto it = css_connected_face_.rbegin(); it != css_connected_face_.rend();
       ++it) {
    if (func.Run(*it)) {
      return true;
    }
  }
  return false;
}

bool FontFaceList::ForEachReverseUntilFalse(
    const base::RepeatingCallback<bool(Member<FontFace>)>& callback) const {
  base::RepeatingCallback<bool(Member<FontFace>)> negating_callback =
      WTF::BindRepeating(NegatingCallback, callback);
  // When |callback| returns |false|,
  // |ForEachReverseUntilTrue(negating_callback)| will stop iterating and return
  // |true|, so negate it.
  return !ForEachReverseUntilTrue(negating_callback);
}

void FontFaceList::ForEachReverse(
    const base::RepeatingCallback<void(Member<FontFace>)>& callback) const {
  base::RepeatingCallback<bool(Member<FontFace>)> false_returning_callback =
      WTF::BindRepeating(FalseReturningCallback, callback);
  ForEachReverseUntilTrue(false_returning_callback);
}

void FontFaceList::Trace(Visitor* visitor) const {
  visitor->Trace(css_connected_face_);
  visitor->Trace(non_css_connected_face_);
}

}  // namespace blink
