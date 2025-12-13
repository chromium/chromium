// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/render_blocking_resource_manager.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/css/font_face.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/html/html_link_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
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
    : element_render_blocking_links_(
          MakeGarbageCollected<RenderBlockingElementLinkMap>(BindRepeating(
              &RenderBlockingResourceManager::OnRenderBlockingElementLinkEmpty,
              WrapWeakPersistent(this)))),
      document_(document),
      font_preload_max_blocking_timer_(
          document.GetTaskRunner(TaskType::kInternalFrameLifecycleControl),
          this,
          &RenderBlockingResourceManager::FontPreloadingTimerFired),
      font_preload_max_fcp_delay_timer_(
          document.GetTaskRunner(TaskType::kInternalFrameLifecycleControl),
          this,
          &RenderBlockingResourceManager::FontPreloadingTimerFired),
      font_preload_timeout_(kMaxRenderingDelayForFontPreloads) {}

void RenderBlockingResourceManager::AddPendingFontPreload(
    const PendingLinkPreload& link) {
  if (font_preload_timer_has_fired_ || document_->body()) {
    return;
  }

  pending_font_preloads_.insert(&link);
  EnsureStartFontPreloadMaxBlockingTimer();
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
  EnsureStartFontPreloadMaxBlockingTimer();
}

void RenderBlockingResourceManager::RemovePendingFontPreload(
    const PendingLinkPreload& link) {
  auto iter = pending_font_preloads_.find(&link);
  if (iter == pending_font_preloads_.end()) {
    return;
  }
  pending_font_preloads_.erase(iter);
  RenderBlockingResourceUnblocked();
}

void RenderBlockingResourceManager::RemoveImperativeFontLoading() {
  if (font_preload_timer_has_fired_)
    return;
  DCHECK(imperative_font_loading_count_);
  --imperative_font_loading_count_;
  RenderBlockingResourceUnblocked();
}

void RenderBlockingResourceManager::EnsureStartFontPreloadMaxBlockingTimer() {
  if (font_preload_timer_has_fired_ ||
      font_preload_max_blocking_timer_.IsActive()) {
    return;
  }
  base::TimeDelta timeout =
      base::FeatureList::IsEnabled(features::kRenderBlockingFonts)
          ? document_->Loader()
                ->RemainingTimeToRenderBlockingFontMaxBlockingTime()
          : font_preload_timeout_;
  font_preload_max_blocking_timer_.StartOneShot(timeout, FROM_HERE);
}

void RenderBlockingResourceManager::FontPreloadingTimerFired(TimerBase*) {
  if (font_preload_timer_has_fired_) {
    return;
  }
  base::UmaHistogramBoolean(
      "WebFont.Clients.RenderBlockingFonts.ExpiredFonts",
      pending_font_preloads_.size() + imperative_font_loading_count_);
  font_preload_timer_has_fired_ = true;
  pending_font_preloads_.clear();
  imperative_font_loading_count_ = 0;
  document_->RenderBlockingResourceUnblocked();
}

void RenderBlockingResourceManager::AddPendingParsingElementLink(
    const AtomicString& id,
    const HTMLLinkElement* link,
    RenderBlockingLevel blocking_level) {
  CHECK(link);

  // We can only add resources until the body element is parsed.
  // Also we need a valid id.
  if (document_->body() || id.empty()) {
    return;
  }

  element_render_blocking_links_->AddLinkWithTargetElement(id, link,
                                                           blocking_level);
  if (blocking_level == RenderBlockingLevel::kBlock) {
    document_->SetHasRenderBlockingExpectLinkElements(true);
  } else if (blocking_level == RenderBlockingLevel::kLimitFrameRate) {
    document_->SetHasFullFrameRateBlockingExpectLinkElements(true);
  }
}

void RenderBlockingResourceManager::RemovePendingParsingElement(
    const AtomicString& id,
    Element* element) {
  // <link rel=expect> matches elements found using "select the indicated part"
  // https://html.spec.whatwg.org/multipage/browsing-the-web.html#select-the-indicated-part
  // which only matches elements in the document tree (as in, not in a shadow
  // tree)
  if (element->IsInShadowTree() || !element->isConnected()) {
    return;
  }
  element_render_blocking_links_->RemoveTargetElement(id);
}

void RenderBlockingResourceManager::RemovePendingParsingElementLink(
    const AtomicString& id,
    const HTMLLinkElement* link) {
  element_render_blocking_links_->RemoveLinkWithTargetElement(id, link);
}

void RenderBlockingResourceManager::ClearPendingParsingElements() {
  if (!element_render_blocking_links_->HasElement(
          RenderBlockingLevel::kBlock) &&
      !element_render_blocking_links_->HasElement(
          RenderBlockingLevel::kLimitFrameRate)) {
    return;
  }
  element_render_blocking_links_->ForEach(BindRepeating(
      [](Document* document, RenderBlockingLevel, const HTMLLinkElement& link) {
        document->AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
            mojom::blink::ConsoleMessageSource::kOther,
            mojom::blink::ConsoleMessageLevel::kWarning,
            StrCat({"Did not find element expected to be parsed from: <link "
                    "rel=expect href=\"",
                    link.FastGetAttribute(html_names::kHrefAttr), "\">"})));
      },
      WrapPersistent(document_.Get())));
  element_render_blocking_links_->Clear();
}

void RenderBlockingResourceManager::OnRenderBlockingElementLinkEmpty(
    RenderBlockingLevel level) {
  if (level == RenderBlockingLevel::kBlock) {
    document_->SetHasRenderBlockingExpectLinkElements(false);
    RenderBlockingResourceUnblocked();
  } else if (level == RenderBlockingLevel::kLimitFrameRate) {
    document_->SetHasFullFrameRateBlockingExpectLinkElements(false);
  }
}

void RenderBlockingResourceManager::SetFontPreloadTimeoutForTest(
    base::TimeDelta timeout) {
  if (font_preload_max_blocking_timer_.IsActive()) {
    font_preload_max_blocking_timer_.Stop();
    font_preload_max_blocking_timer_.StartOneShot(timeout, FROM_HERE);
  }
  font_preload_timeout_ = timeout;
}

void RenderBlockingResourceManager::DisableFontPreloadTimeoutForTest() {
  if (font_preload_max_blocking_timer_.IsActive()) {
    font_preload_max_blocking_timer_.Stop();
  }
}

bool RenderBlockingResourceManager::FontPreloadTimerIsActiveForTest() const {
  return font_preload_max_blocking_timer_.IsActive();
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
  RenderBlockingResourceUnblocked();
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
  RenderBlockingResourceUnblocked();
}

void RenderBlockingResourceManager::WillInsertDocumentBody() {
  if (base::FeatureList::IsEnabled(features::kRenderBlockingFonts) &&
      !HasNonFontRenderBlockingResources() && HasRenderBlockingFonts()) {
    EnsureStartFontPreloadMaxFCPDelayTimer();
  }
}

void RenderBlockingResourceManager::RenderBlockingResourceUnblocked() {
  document_->RenderBlockingResourceUnblocked();
  if (base::FeatureList::IsEnabled(features::kRenderBlockingFonts) &&
      !HasNonFontRenderBlockingResources() && HasRenderBlockingFonts() &&
      document_->body()) {
    EnsureStartFontPreloadMaxFCPDelayTimer();
  }
}

void RenderBlockingResourceManager::EnsureStartFontPreloadMaxFCPDelayTimer() {
  if (font_preload_timer_has_fired_ ||
      font_preload_max_fcp_delay_timer_.IsActive()) {
    return;
  }
  base::TimeDelta max_fcp_delay =
      base::Milliseconds(features::kMaxFCPDelayMsForRenderBlockingFonts.Get());
  font_preload_max_fcp_delay_timer_.StartOneShot(max_fcp_delay, FROM_HERE);
}

void RenderBlockingResourceManager::Trace(Visitor* visitor) const {
  visitor->Trace(element_render_blocking_links_);
  visitor->Trace(document_);
  visitor->Trace(pending_stylesheet_owner_nodes_);
  visitor->Trace(pending_scripts_);
  visitor->Trace(pending_font_preloads_);
  visitor->Trace(font_preload_max_blocking_timer_);
  visitor->Trace(font_preload_max_fcp_delay_timer_);
}

}  // namespace blink
