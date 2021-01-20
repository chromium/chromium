// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/font_preload_manager.h"

#include "build/build_config.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/css/font_face.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/resource/font_resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_finish_observer.h"

namespace blink {

namespace {

class FontPreloadFinishObserver final : public ResourceFinishObserver {
 public:
  FontPreloadFinishObserver(FontResource& font_resource, Document& document)
      : font_resource_(font_resource), document_(document) {}

  ~FontPreloadFinishObserver() final = default;

  void Trace(blink::Visitor* visitor) const final {
    visitor->Trace(font_resource_);
    visitor->Trace(document_);
    ResourceFinishObserver::Trace(visitor);
  }

 private:
  void NotifyFinished() final {
    document_->GetFontPreloadManager().FontPreloadingFinished(font_resource_,
                                                              this);
  }

  String DebugName() const final { return "FontPreloadFinishObserver"; }

  Member<FontResource> font_resource_;
  Member<Document> document_;
};

class ImperativeFontLoadFinishedCallback final
    : public GarbageCollected<ImperativeFontLoadFinishedCallback>,
      public FontFace::LoadFontCallback {
 public:
  explicit ImperativeFontLoadFinishedCallback(Document& document)
      : document_(document) {}
  ~ImperativeFontLoadFinishedCallback() final = default;

  void Trace(Visitor* visitor) const final {
    visitor->Trace(document_);
    FontFace::LoadFontCallback::Trace(visitor);
  }

 private:
  void NotifyLoaded(FontFace*) final {
    document_->GetFontPreloadManager().ImperativeFontLoadingFinished();
  }

  void NotifyError(FontFace*) final {
    document_->GetFontPreloadManager().ImperativeFontLoadingFinished();
  }

  Member<Document> document_;
};

}  // namespace

FontPreloadManager::FontPreloadManager(Document& document)
    : document_(document),
      render_delay_timer_(
          document.GetTaskRunner(TaskType::kInternalFrameLifecycleControl),
          this,
          &FontPreloadManager::FontPreloadingDelaysRenderingTimerFired),
      render_delay_timeout_(base::TimeDelta::FromMilliseconds(
          features::kFontPreloadingDelaysRenderingParam.Get())) {}

bool FontPreloadManager::HasPendingRenderBlockingFonts() const {
  return state_ == State::kLoading;
}

void FontPreloadManager::FontPreloadingStarted(FontResource* font_resource) {
  // The font is either already in the memory cache, or has errored out. In
  // either case, we don't any further processing.
  if (font_resource->IsLoaded())
    return;

  if (state_ == State::kUnblocked)
    return;

  if (!base::FeatureList::IsEnabled(features::kFontPreloadingDelaysRendering))
    return;

  FontPreloadFinishObserver* observer =
      MakeGarbageCollected<FontPreloadFinishObserver>(*font_resource,
                                                      *document_);
  font_resource->AddFinishObserver(
      observer, document_->GetTaskRunner(TaskType::kInternalLoading).get());
  finish_observers_.insert(observer);

  RenderBlockingFontLoadingStarted();
}

void FontPreloadManager::ImperativeFontLoadingStarted(FontFace* font_face) {
  if (font_face->LoadStatus() != FontFace::kLoading)
    return;

  if (state_ == State::kUnblocked)
    return;

  if (!base::FeatureList::IsEnabled(features::kFontPreloadingDelaysRendering))
    return;

  ImperativeFontLoadFinishedCallback* callback =
      MakeGarbageCollected<ImperativeFontLoadFinishedCallback>(*document_);
  font_face->AddCallback(callback);
  ++imperative_font_loading_count_;

  RenderBlockingFontLoadingStarted();
}

void FontPreloadManager::RenderBlockingFontLoadingStarted() {
  DCHECK(
      base::FeatureList::IsEnabled(features::kFontPreloadingDelaysRendering));
  DCHECK_NE(State::kUnblocked, state_);
  if (state_ == State::kInitial)
    render_delay_timer_.StartOneShot(render_delay_timeout_, FROM_HERE);
  state_ = State::kLoading;
}

void FontPreloadManager::FontPreloadingFinished(
    FontResource* font_resource,
    ResourceFinishObserver* observer) {
  DCHECK(
      base::FeatureList::IsEnabled(features::kFontPreloadingDelaysRendering));
  if (state_ == State::kUnblocked) {
    finish_observers_.clear();
    return;
  }

  DCHECK(finish_observers_.Contains(observer));
  finish_observers_.erase(observer);
  RenderBlockingFontLoadingFinished();
}

void FontPreloadManager::ImperativeFontLoadingFinished() {
  DCHECK(
      base::FeatureList::IsEnabled(features::kFontPreloadingDelaysRendering));
  if (state_ == State::kUnblocked) {
    imperative_font_loading_count_ = 0;
    return;
  }

  DCHECK(imperative_font_loading_count_);
  --imperative_font_loading_count_;
  RenderBlockingFontLoadingFinished();
}

void FontPreloadManager::RenderBlockingFontLoadingFinished() {
  DCHECK(
      base::FeatureList::IsEnabled(features::kFontPreloadingDelaysRendering));
  DCHECK_NE(State::kUnblocked, state_);
  if (!finish_observers_.IsEmpty() || imperative_font_loading_count_)
    return;
  state_ = State::kLoaded;
  document_->FontPreloadingFinishedOrTimedOut();
}

void FontPreloadManager::WillBeginRendering() {
  if (state_ == State::kUnblocked)
    return;

  state_ = State::kUnblocked;
  finish_observers_.clear();
  imperative_font_loading_count_ = 0;
}

void FontPreloadManager::FontPreloadingDelaysRenderingTimerFired(TimerBase*) {
  if (state_ == State::kUnblocked)
    return;

  WillBeginRendering();
  document_->FontPreloadingFinishedOrTimedOut();
}

void FontPreloadManager::SetRenderDelayTimeoutForTest(base::TimeDelta timeout) {
  if (render_delay_timer_.IsActive()) {
    render_delay_timer_.Stop();
    render_delay_timer_.StartOneShot(timeout, FROM_HERE);
  }
  render_delay_timeout_ = timeout;
}

void FontPreloadManager::DisableTimeoutForTest() {
  if (render_delay_timer_.IsActive())
    render_delay_timer_.Stop();
}

void FontPreloadManager::Trace(Visitor* visitor) const {
  visitor->Trace(document_);
  visitor->Trace(finish_observers_);
  visitor->Trace(render_delay_timer_);
}

}  // namespace blink
