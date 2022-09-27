// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/render_blocking_resource_manager.h"

#include "third_party/blink/renderer/core/css/font_face.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/loader/pending_link_preload.h"
#include "third_party/blink/renderer/core/script/script_element_base.h"

namespace blink {

namespace {

// 50ms is the overall best performing value in our experiments.
const base::TimeDelta kMaxRenderingDelayForFontPreloads =
    base::Milliseconds(50);

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
        ->RemoveImperativeFontLoading();
  }

  void NotifyError(FontFace*) final {
    DCHECK(document_->GetRenderBlockingResourceManager());
    document_->GetRenderBlockingResourceManager()
        ->RemoveImperativeFontLoading();
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
      font_preload_timeout_(kMaxRenderingDelayForFontPreloads) {}

void RenderBlockingResourceManager::AddPendingPreload(
    const PendingLinkPreload& link,
    PreloadType type) {
  // TODO(crbug.com/1271296): `kRegular` is no longer in use. Clean up the code.
  DCHECK_EQ(type, PreloadType::kShortBlockingFont);

  if (type == PreloadType::kShortBlockingFont && font_preload_timer_has_fired_)
    return;

  if (document_->body())
    return;

  pending_preloads_.insert(&link, type);
  if (type == PreloadType::kShortBlockingFont)
    EnsureStartFontPreloadTimer();
}

void RenderBlockingResourceManager::AddImperativeFontLoading(
    FontFace* font_face) {
  if (font_face->LoadStatus() != FontFace::kLoading)
    return;

  if (font_preload_timer_has_fired_ || document_->body())
    return;

  ImperativeFontLoadFinishedCallback* callback =
      MakeGarbageCollected<ImperativeFontLoadFinishedCallback>(*document_);
  font_face->AddCallback(callback);
  ++imperative_font_loading_count_;
  EnsureStartFontPreloadTimer();
}

void RenderBlockingResourceManager::RemovePendingPreload(
    const PendingLinkPreload& link) {
  auto iter = pending_preloads_.find(&link);
  if (iter == pending_preloads_.end())
    return;
  pending_preloads_.erase(iter);
  document_->RenderBlockingResourceUnblocked();
}

void RenderBlockingResourceManager::RemoveImperativeFontLoading() {
  if (font_preload_timer_has_fired_)
    return;
  DCHECK(imperative_font_loading_count_);
  --imperative_font_loading_count_;
  document_->RenderBlockingResourceUnblocked();
}

void RenderBlockingResourceManager::EnsureStartFontPreloadTimer() {
  if (!font_preload_timer_.IsActive())
    font_preload_timer_.StartOneShot(font_preload_timeout_, FROM_HERE);
}

void RenderBlockingResourceManager::FontPreloadingTimerFired(TimerBase*) {
  font_preload_timer_has_fired_ = true;
  VectorOf<const PendingLinkPreload> short_blocking_font_preloads;
  for (auto preload_and_type : pending_preloads_) {
    if (preload_and_type.value == PreloadType::kShortBlockingFont)
      short_blocking_font_preloads.push_back(preload_and_type.key);
  }
  pending_preloads_.RemoveAll(short_blocking_font_preloads);
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

bool RenderBlockingResourceManager::FontPreloadTimerIsActiveForTest() const {
  return font_preload_timer_.IsActive();
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
  visitor->Trace(pending_preloads_);
  visitor->Trace(font_preload_timer_);
}

}  // namespace blink
