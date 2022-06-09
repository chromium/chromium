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
#include "third_party/blink/renderer/core/css/remote_font_face_source.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/fonts/simple_font_data.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

CSSFontFace::CSSFontFace(FontFace* font_face, Vector<UnicodeRange>& ranges)
    : ranges_(base::AdoptRef(new UnicodeRangeSet(ranges))),
      font_face_(font_face)
#if defined(USE_PARALLEL_TEXT_SHAPING)
      ,
      task_runner_(font_face->GetExecutionContext()->GetTaskRunner(
          TaskType::kFontLoading))
#endif
{
  DCHECK(font_face_);
}

void CSSFontFace::AddSource(CSSFontFaceSource* source) {
  AutoLockForParallelTextShaping guard(sources_lock_);
  sources_.push_back(source);
}

void CSSFontFace::AddSegmentedFontFace(
    CSSSegmentedFontFace* segmented_font_face) {
  DCHECK(IsContextThread());
  DCHECK(!segmented_font_faces_.Contains(segmented_font_face));
  segmented_font_faces_.insert(segmented_font_face);
}

void CSSFontFace::RemoveSegmentedFontFace(
    CSSSegmentedFontFace* segmented_font_face) {
  DCHECK(IsContextThread());
  DCHECK(segmented_font_faces_.Contains(segmented_font_face));
  segmented_font_faces_.erase(segmented_font_face);
}

void CSSFontFace::DidBeginLoad() {
  DCHECK(IsContextThread());
  if (LoadStatus() == FontFace::kUnloaded)
    SetLoadStatus(FontFace::kLoading);
}

bool CSSFontFace::FontLoaded(CSSFontFaceSource* source) {
  DCHECK(IsContextThread());
  if (source != FrontSource())
    return false;

  if (LoadStatus() == FontFace::kLoading) {
    if (source->IsValid()) {
      SetLoadStatus(FontFace::kLoaded);
    } else if (source->IsInFailurePeriod()) {
      {
        AutoLockForParallelTextShaping guard(sources_lock_);
        sources_.clear();
      }
      SetLoadStatus(FontFace::kError);
    } else {
      {
        AutoLockForParallelTextShaping guard(sources_lock_);
        if (!sources_.IsEmpty() && source == sources_.front())
          sources_.pop_front();
      }
      Load();
    }
  }

  for (const auto& segmented_font_face : segmented_font_faces_)
    segmented_font_face->FontFaceInvalidated();
  return true;
}

void CSSFontFace::SetDisplay(FontDisplay value) {
  for (auto& source : GetSources()) {
    source->SetDisplay(value);
  }
}

size_t CSSFontFace::ApproximateBlankCharacterCount() const {
  auto* const source = FrontSource();
  if (!source || !source->IsInBlockPeriod())
    return 0;
  size_t approximate_character_count_ = 0;
  for (const auto& segmented_font_face : segmented_font_faces_) {
    approximate_character_count_ +=
        segmented_font_face->ApproximateCharacterCount();
  }
  return approximate_character_count_;
}

bool CSSFontFace::FallbackVisibilityChanged(RemoteFontFaceSource* source) {
  if (source != FrontSource())
    return false;
  for (const auto& segmented_font_face : segmented_font_faces_)
    segmented_font_face->FontFaceInvalidated();
  return true;
}

scoped_refptr<SimpleFontData> CSSFontFace::GetFontData(
    const FontDescription& font_description) {
  AutoLockForParallelTextShaping guard(sources_lock_);

  if (sources_.IsEmpty())
    return nullptr;

  // Apply the 'size-adjust' descriptor before font selection.
  // https://drafts.csswg.org/css-fonts-5/#descdef-font-face-size-adjust
  const FontDescription& size_adjusted_description =
      font_face_->HasSizeAdjust()
          ? font_description.SizeAdjustedFontDescription(
                font_face_->GetSizeAdjust())
          : font_description;

  // https://www.w3.org/TR/css-fonts-4/#src-desc
  // "When a font is needed the user agent iterates over the set of references
  // listed, using the first one it can successfully activate."
  while (!sources_.IsEmpty()) {
    Member<CSSFontFaceSource>& source = sources_.front();

    // Bail out if the first source is in the Failure period, causing fallback
    // to next font-family.
    if (source->IsInFailurePeriod())
      return nullptr;

    if (scoped_refptr<SimpleFontData> result =
            source->GetFontData(size_adjusted_description,
                                font_face_->GetFontSelectionCapabilities())) {
      if (font_face_->HasFontMetricsOverride()) {
        // TODO(xiaochengh): Try not to create a temporary
        // SimpleFontData.
        result = result->MetricsOverriddenFontData(
            font_face_->GetFontMetricsOverride());
      }
      // The active source may already be loading or loaded. Adjust our
      // FontFace status accordingly.
      UpdateLoadStatusForActiveSource(source);
      return result;
    }
    sources_.pop_front();
  }

  // We ran out of source. Set the FontFace status to "error" and return.
  UpdateLoadStatusForNoSource();
  return nullptr;
}

void CSSFontFace::UpdateLoadStatusForActiveSource(CSSFontFaceSource* source) {
#if defined(USE_PARALLEL_TEXT_SHAPING)
  if (auto task_runner = GetCrossThreadTaskRunner()) {
    PostCrossThreadTask(
        *task_runner, FROM_HERE,
        CrossThreadBindOnce(&CSSFontFace::UpdateLoadStatusForActiveSource,
                            WrapCrossThreadPersistent(this),
                            WrapCrossThreadPersistent(source)));
    return;
  }
  if (!font_face_->GetExecutionContext())
    return;
  DCHECK(IsContextThread());
#endif
  if (LoadStatus() == FontFace::kUnloaded &&
      (source->IsLoading() || source->IsLoaded()))
    SetLoadStatus(FontFace::kLoading);
  if (LoadStatus() == FontFace::kLoading && source->IsLoaded())
    SetLoadStatus(FontFace::kLoaded);
}

void CSSFontFace::UpdateLoadStatusForNoSource() {
#if defined(USE_PARALLEL_TEXT_SHAPING)
  if (auto task_runner = GetCrossThreadTaskRunner()) {
    PostCrossThreadTask(
        *task_runner, FROM_HERE,
        CrossThreadBindOnce(&CSSFontFace::UpdateLoadStatusForNoSource,
                            WrapCrossThreadPersistent(this)));
    return;
  }
  if (!font_face_->GetExecutionContext())
    return;
  DCHECK(IsContextThread());
#endif
  if (LoadStatus() == FontFace::kUnloaded)
    SetLoadStatus(FontFace::kLoading);
  if (LoadStatus() == FontFace::kLoading)
    SetLoadStatus(FontFace::kError);
}

bool CSSFontFace::MaybeLoadFont(const FontDescription& font_description,
                                const StringView& text) {
  DCHECK(IsContextThread());
  // This is a fast path of loading web font in style phase. For speed, this
  // only checks if the first character of the text is included in the font's
  // unicode range. If this font is needed by subsequent characters, load is
  // kicked off in layout phase.
  const UChar32 character = text.length() ? text.CodepointAt(0) : 0;
  if (!ranges_->Contains(character))
    return false;
  if (LoadStatus() != FontFace::kUnloaded)
    return true;
  LoadInternal(font_description);
  return true;
}

bool CSSFontFace::MaybeLoadFont(const FontDescription& font_description,
                                const FontDataForRangeSet& range_set) {
  if (ranges_ != range_set.Ranges())
    return false;
  if (LoadStatus() != FontFace::kUnloaded)
    return true;
  LoadInternal(font_description);
  return true;
}

void CSSFontFace::Load() {
  DCHECK(IsContextThread());
  FontDescription font_description;
  FontFamily font_family;
  font_family.SetFamily(font_face_->family(), FontFamily::Type::kFamilyName);
  font_description.SetFamily(font_family);
  LoadInternal(font_description);
}

void CSSFontFace::LoadInternal(const FontDescription& font_description) {
#if defined(USE_PARALLEL_TEXT_SHAPING)
  if (auto task_runner = GetCrossThreadTaskRunner()) {
    PostCrossThreadTask(
        *task_runner, FROM_HERE,
        CrossThreadBindOnce(&CSSFontFace::LoadInternal,
                            WrapCrossThreadPersistent(this), font_description));
    return;
  }
#endif
  if (LoadStatus() == FontFace::kUnloaded)
    SetLoadStatus(FontFace::kLoading);
  DCHECK_EQ(LoadStatus(), FontFace::kLoading);

  AutoLockForParallelTextShaping guard(sources_lock_);
  while (!sources_.IsEmpty()) {
    Member<CSSFontFaceSource>& source = sources_.front();
    if (source->IsValid()) {
      if (source->IsLocalNonBlocking()) {
        if (source->IsLocalFontAvailable(font_description)) {
          SetLoadStatus(FontFace::kLoaded);
          return;
        }
      } else {
        if (!source->IsLoaded())
          source->BeginLoadIfNeeded();
        else
          SetLoadStatus(FontFace::kLoaded);
        return;
      }
    }
    sources_.pop_front();
  }
  SetLoadStatus(FontFace::kError);
}

void CSSFontFace::SetLoadStatus(FontFace::LoadStatusType new_status) {
  DCHECK(IsContextThread());
  DCHECK(font_face_);
  if (new_status == FontFace::kError)
    font_face_->SetError();
  else
    font_face_->SetLoadStatus(new_status);

  if (segmented_font_faces_.IsEmpty() || !font_face_->GetExecutionContext())
    return;

  if (auto* window =
          DynamicTo<LocalDOMWindow>(font_face_->GetExecutionContext())) {
    if (new_status == FontFace::kLoading) {
      FontFaceSetDocument::From(*window->document())
          ->BeginFontLoading(font_face_);
    }
  } else if (auto* scope = DynamicTo<WorkerGlobalScope>(
                 font_face_->GetExecutionContext())) {
    if (new_status == FontFace::kLoading)
      FontFaceSetWorker::From(*scope)->BeginFontLoading(font_face_);
  }
}

bool CSSFontFace::UpdatePeriod() {
  if (LoadStatus() == FontFace::kLoaded)
    return false;
  bool changed = false;
  for (CSSFontFaceSource* source : GetSources()) {
    if (source->UpdatePeriod())
      changed = true;
  }
  return changed;
}

void CSSFontFace::Trace(Visitor* visitor) const {
  {
    AutoLockForParallelTextShaping guard(sources_lock_);
    visitor->Trace(sources_);
  }
  visitor->Trace(font_face_);
}

bool CSSFontFace::IsContextThread() const {
#if defined(USE_PARALLEL_TEXT_SHAPING)
  return font_face_->GetExecutionContext()->IsContextThread();
#else
  return true;
#endif
}

#if defined(USE_PARALLEL_TEXT_SHAPING)
scoped_refptr<base::SequencedTaskRunner> CSSFontFace::GetCrossThreadTaskRunner()
    const {
  auto* const context = font_face_->GetExecutionContext();
  if (!context || context->IsContextThread())
    return nullptr;
  return task_runner_;
}
#endif

HeapVector<Member<CSSFontFaceSource>> CSSFontFace::GetSources() const {
  AutoLockForParallelTextShaping guard(sources_lock_);
  HeapVector<Member<CSSFontFaceSource>> sources;
  CopyToVector(sources_, sources);
  return sources;
}

}  // namespace blink
