// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/render_blocking_resource_manager.h"

#include "build/build_config.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/css/font_face.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/resource/font_resource.h"
#include "third_party/blink/renderer/core/script/script_element_base.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_finish_observer.h"

namespace blink {

namespace {

// 50ms is the overall best performing value in our experiments.
const base::TimeDelta kMaxRenderingDelay = base::Milliseconds(50);

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
    DCHECK(document_->GetRenderBlockingResourceManager());
    document_->GetRenderBlockingResourceManager()->FontPreloadingFinished(
        font_resource_, this);
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
    DCHECK(document_->GetRenderBlockingResourceManager());
    document_->GetRenderBlockingResourceManager()
        ->ImperativeFontLoadingFinished();
  }

  void NotifyError(FontFace*) final {
    DCHECK(document_->GetRenderBlockingResourceManager());
    document_->GetRenderBlockingResourceManager()
        ->ImperativeFontLoadingFinished();
  }

  Member<Document> document_;
};

}  // namespace

RenderBlockingResourceManager::RenderBlockingResourceManager(Document& document)
    : document_(document),
      font_preload_timer_(
          document.GetTaskRunner(TaskType::kInternalFrameLifecycleControl),
          this,
          &RenderBlockingResourceManager::FontPreloadingTimerFired),
      font_preload_timeout_(kMaxRenderingDelay) {}

void RenderBlockingResourceManager::FontPreloadingStarted(
    FontResource* font_resource) {
  // The font is either already in the memory cache, or has errored out. In
  // either case, we don't any further processing.
  if (font_resource->IsLoaded())
    return;

  if (font_preload_timer_has_fired_ || document_->body())
    return;

  FontPreloadFinishObserver* observer =
      MakeGarbageCollected<FontPreloadFinishObserver>(*font_resource,
                                                      *document_);
  font_resource->AddFinishObserver(
      observer, document_->GetTaskRunner(TaskType::kInternalLoading).get());
  font_preload_finish_observers_.insert(observer);

  if (!font_preload_timer_.IsActive())
    font_preload_timer_.StartOneShot(font_preload_timeout_, FROM_HERE);
}

void RenderBlockingResourceManager::ImperativeFontLoadingStarted(
    FontFace* font_face) {
  if (font_face->LoadStatus() != FontFace::kLoading)
    return;

  if (font_preload_timer_has_fired_ || document_->body())
    return;

  ImperativeFontLoadFinishedCallback* callback =
      MakeGarbageCollected<ImperativeFontLoadFinishedCallback>(*document_);
  font_face->AddCallback(callback);
  ++imperative_font_loading_count_;

  if (!font_preload_timer_.IsActive())
    font_preload_timer_.StartOneShot(font_preload_timeout_, FROM_HERE);
}

void RenderBlockingResourceManager::FontPreloadingFinished(
    FontResource* font_resource,
    ResourceFinishObserver* observer) {
  if (font_preload_timer_has_fired_)
    return;
  DCHECK(font_preload_finish_observers_.Contains(observer));
  font_preload_finish_observers_.erase(observer);
  document_->RenderBlockingResourceUnblocked();
}

void RenderBlockingResourceManager::ImperativeFontLoadingFinished() {
  if (font_preload_timer_has_fired_)
    return;
  DCHECK(imperative_font_loading_count_);
  --imperative_font_loading_count_;
  document_->RenderBlockingResourceUnblocked();
}

void RenderBlockingResourceManager::FontPreloadingTimerFired(TimerBase*) {
  font_preload_timer_has_fired_ = true;
  font_preload_finish_observers_.clear();
  imperative_font_loading_count_ = 0;
  document_->RenderBlockingResourceUnblocked();
}

void RenderBlockingResourceManager::SetFontPreloadTimeoutForTest(
    base::TimeDelta timeout) {
  if (font_preload_timer_.IsActive()) {
    font_preload_timer_.Stop();
    font_preload_timer_.StartOneShot(timeout, FROM_HERE);
  }
  font_preload_timeout_ = timeout;
}

void RenderBlockingResourceManager::DisableFontPreloadTimeoutForTest() {
  if (font_preload_timer_.IsActive())
    font_preload_timer_.Stop();
}

bool RenderBlockingResourceManager::AddPendingStylesheet(
    const Node& owner_node) {
  if (document_->body())
    return false;
  DCHECK(!pending_stylesheet_owner_nodes_.Contains(&owner_node));
  pending_stylesheet_owner_nodes_.insert(&owner_node);
  return true;
}

bool RenderBlockingResourceManager::RemovePendingStylesheet(
    const Node& owner_node) {
  auto iter = pending_stylesheet_owner_nodes_.find(&owner_node);
  if (iter == pending_stylesheet_owner_nodes_.end())
    return false;
  pending_stylesheet_owner_nodes_.erase(iter);
  document_->RenderBlockingResourceUnblocked();
  return true;
}

void RenderBlockingResourceManager::AddPendingScript(
    const ScriptElementBase& script) {
  if (document_->body())
    return;
  pending_scripts_.insert(&script);
}

void RenderBlockingResourceManager::RemovePendingScript(
    const ScriptElementBase& script) {
  auto iter = pending_scripts_.find(&script);
  if (iter == pending_scripts_.end())
    return;
  pending_scripts_.erase(iter);
  document_->RenderBlockingResourceUnblocked();
}

void RenderBlockingResourceManager::Trace(Visitor* visitor) const {
  visitor->Trace(document_);
  visitor->Trace(pending_stylesheet_owner_nodes_);
  visitor->Trace(pending_scripts_);
  visitor->Trace(font_preload_finish_observers_);
  visitor->Trace(font_preload_timer_);
}

}  // namespace blink
