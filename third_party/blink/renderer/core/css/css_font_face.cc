/*
 * Copyright (C) 2007, 2008, 2011 Apple Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/css/css_font_face.h"

#include <algorithm>
#include "third_party/blink/renderer/core/css/css_font_face_source.h"
#include "third_party/blink/renderer/core/css/css_font_selector.h"
#include "third_party/blink/renderer/core/css/css_segmented_font_face.h"
#include "third_party/blink/renderer/core/css/font_face_set_document.h"
#include "third_party/blink/renderer/core/css/font_face_set_worker.h"
#include "third_party/blink/renderer/core/css/font_size_functions.h"
#include "third_party/blink/renderer/core/css/remote_font_face_source.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

void CSSFontFace::AddSource(CSSFontFaceSource* source) {
  sources_.push_back(source);
}

void CSSFontFace::AddSegmentedFontFace(
    CSSSegmentedFontFace* segmented_font_face) {
  DCHECK(!segmented_font_faces_.Contains(segmented_font_face));
  segmented_font_faces_.insert(segmented_font_face);
}

void CSSFontFace::RemoveSegmentedFontFace(
    CSSSegmentedFontFace* segmented_font_face) {
  DCHECK(segmented_font_faces_.Contains(segmented_font_face));
  segmented_font_faces_.erase(segmented_font_face);
}

void CSSFontFace::DidBeginLoad() {
  if (LoadStatus() == FontFace::kUnloaded) {
    SetLoadStatus(FontFace::kLoading);
  }
}

bool CSSFontFace::FontLoaded(CSSFontFaceSource* source) {
  if (!IsValid() || source != sources_.front()) {
    return false;
  }

  if (LoadStatus() == FontFace::kLoading) {
    if (source->IsValid()) {
      SetLoadStatus(FontFace::kLoaded);
    } else if (source->IsInFailurePeriod()) {
      sources_.clear();
      SetLoadStatus(FontFace::kError);
    } else {
      sources_.pop_front();
      Load();
    }
  }

  for (CSSSegmentedFontFace* segmented_font_face : segmented_font_faces_) {
    segmented_font_face->FontFaceInvalidated();
  }
  return true;
}

void CSSFontFace::SetDisplay(FontDisplay value) {
  for (auto& source : sources_) {
    source->SetDisplay(value);
  }
}

size_t CSSFontFace::ApproximateBlankCharacterCount() const {
  if (sources_.empty() || !sources_.front()->IsInBlockPeriod()) {
    return 0;
  }
  size_t approximate_character_count_ = 0;
  for (CSSSegmentedFontFace* segmented_font_face : segmented_font_faces_) {
    approximate_character_count_ +=
        segmented_font_face->ApproximateCharacterCount();
  }
  return approximate_character_count_;
}

bool CSSFontFace::FallbackVisibilityChanged(RemoteFontFaceSource* source) {
  if (!IsValid() || source != sources_.front()) {
    return false;
  }
  for (CSSSegmentedFontFace* segmented_font_face : segmented_font_faces_) {
    segmented_font_face->FontFaceInvalidated();
  }
  return true;
}

const SimpleFontData* CSSFontFace::GetFontData(
    const FontDescription& font_description) {
  if (!IsValid()) {
    return nullptr;
  }

  // Apply the 'size-adjust' descriptor before font selection.
  // https://drafts.csswg.org/css-fonts-5/#descdef-font-face-size-adjust
  FontDescription size_adjusted_description =
      font_face_->HasSizeAdjust()
          ? font_description.SizeAdjustedFontDescription(
                font_face_->GetSizeAdjust())
          : font_description;

  // https://www.w3.org/TR/css-fonts-4/#src-desc
  // "When a font is needed the user agent iterates over the set of references
  // listed, using the first one it can successfully activate."
  while (!sources_.empty()) {
    Member<CSSFontFaceSource>& source = sources_.front();

    // Bail out if the first source is in the Failure period, causing fallback
    // to next font-family.
    if (source->IsInFailurePeriod()) {
      return nullptr;
    }

    if (const SimpleFontData* result =
            source->GetFontData(size_adjusted_description,
                                font_face_->GetFontSelectionCapabilities())) {
      // The font data here is created using the primary font's description.
      // We need to adjust the size of a fallback font with actual font metrics
      // if the description has font-size-adjust.
      if (size_adjusted_description.HasSizeAdjust()) {
        if (auto adjusted_size =
                FontSizeFunctions::MetricsMultiplierAdjustedFontSize(
                    result, size_adjusted_description)) {
          size_adjusted_description.SetAdjustedSize(adjusted_size.value());
          result =
              source->GetFontData(size_adjusted_description,
                                  font_face_->GetFontSelectionCapabilities());
        }
      }

      if (font_face_->HasFontMetricsOverride()) {
        // TODO(xiaochengh): Try not to create a temporary
        // SimpleFontData.
        result = result->MetricsOverriddenFontData(
            font_face_->GetFontMetricsOverride());
      }
      // The active source may already be loading or loaded. Adjust our
      // FontFace status accordingly.
      if (LoadStatus() == FontFace::kUnloaded &&
          (source->IsLoading() || source->IsLoaded())) {
        SetLoadStatus(FontFace::kLoading);
      }
      if (LoadStatus() == FontFace::kLoading && source->IsLoaded()) {
        SetLoadStatus(FontFace::kLoaded);
      }
      return result;
    }
    sources_.pop_front();
  }

  // We ran out of source. Set the FontFace status to "error" and return.
  if (LoadStatus() == FontFace::kUnloaded) {
    SetLoadStatus(FontFace::kLoading);
  }
  if (LoadStatus() == FontFace::kLoading) {
    SetLoadStatus(FontFace::kError);
  }
  return nullptr;
}

bool CSSFontFace::MaybeLoadFont(const FontDescription& font_description,
                                const String& text) {
  // This is a fast path of loading web font in style phase. For speed, this
  // only checks if the first character of the text is included in the font's
  // unicode range. If this font is needed by subsequent characters, load is
  // kicked off in layout phase.
  UChar32 character = text.CharacterStartingAt(0);
  if (ranges_->Contains(character)) {
    if (LoadStatus() == FontFace::kUnloaded) {
      Load(font_description);
    }
    return true;
  }
  return false;
}

bool CSSFontFace::MaybeLoadFont(const FontDescription& font_description,
                                const FontDataForRangeSet& range_set) {
  if (ranges_ == range_set.Ranges()) {
    if (LoadStatus() == FontFace::kUnloaded) {
      Load(font_description);
    }
    return true;
  }
  return false;
}

void CSSFontFace::Load() {
  FontDescription font_description;
  font_description.SetFamily(
      FontFamily(font_face_->family(), FontFamily::Type::kFamilyName));
  Load(font_description);
}

void CSSFontFace::Load(const FontDescription& font_description) {
  if (LoadStatus() == FontFace::kUnloaded) {
    SetLoadStatus(FontFace::kLoading);
  }
  DCHECK_EQ(LoadStatus(), FontFace::kLoading);

  while (!sources_.empty()) {
    Member<CSSFontFaceSource>& source = sources_.front();
    if (source->IsValid()) {
      if (source->IsLocalNonBlocking()) {
        if (source->IsLocalFontAvailable(font_description)) {
          SetLoadStatus(FontFace::kLoaded);
          return;
        }
      } else {
        if (!source->IsLoaded()) {
          source->BeginLoadIfNeeded();
        } else {
          SetLoadStatus(FontFace::kLoaded);
        }
        return;
      }
    }
    sources_.pop_front();
  }
  SetLoadStatus(FontFace::kError);
}

void CSSFontFace::SetLoadStatus(FontFace::LoadStatusType new_status) {
  DCHECK(font_face_);
  if (new_status == FontFace::kError) {
    font_face_->SetError();
  } else {
    font_face_->SetLoadStatus(new_status);
  }

  if (segmented_font_faces_.empty() || !font_face_->GetExecutionContext()) {
    return;
  }

  if (auto* window =
          DynamicTo<LocalDOMWindow>(font_face_->GetExecutionContext())) {
    if (new_status == FontFace::kLoading) {
      FontFaceSetDocument::From(*window->document())
          ->BeginFontLoading(font_face_);
    }
  } else if (auto* scope = DynamicTo<WorkerGlobalScope>(
                 font_face_->GetExecutionContext())) {
    if (new_status == FontFace::kLoading) {
      FontFaceSetWorker::From(*scope)->BeginFontLoading(font_face_);
    }
  }
}

bool CSSFontFace::UpdatePeriod() {
  if (LoadStatus() == FontFace::kLoaded) {
    return false;
  }
  bool changed = false;
  for (CSSFontFaceSource* source : sources_) {
    if (source->UpdatePeriod()) {
      changed = true;
    }
  }
  return changed;
}

void CSSFontFace::Trace(Visitor* visitor) const {
  visitor->Trace(segmented_font_faces_);
  visitor->Trace(sources_);
  visitor->Trace(ranges_);
  visitor->Trace(font_face_);
}

}  // namespace blink
