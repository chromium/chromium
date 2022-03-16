// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_RENDER_BLOCKING_RESOURCE_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_RENDER_BLOCKING_RESOURCE_MANAGER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/timer.h"

namespace blink {

class Document;
class FontResource;
class FontFace;
class Node;
class ResourceFinishObserver;
class ScriptElementBase;

// https://html.spec.whatwg.org/#render-blocking-mechanism with some extensions.
class CORE_EXPORT RenderBlockingResourceManager final
    : public GarbageCollected<RenderBlockingResourceManager> {
 public:
  explicit RenderBlockingResourceManager(Document&);
  ~RenderBlockingResourceManager() = default;

  RenderBlockingResourceManager(const RenderBlockingResourceManager&) = delete;
  RenderBlockingResourceManager& operator=(
      const RenderBlockingResourceManager&) = delete;

  bool HasRenderBlockingResources() const {
    return pending_stylesheet_owner_nodes_.size() || pending_scripts_.size() ||
           font_preload_finish_observers_.size() ||
           imperative_font_loading_count_;
  }

  // TODO(crbug.com/1271296): Use this class to handle render-blocking preloads.

  bool HasPendingStylesheets() const {
    return pending_stylesheet_owner_nodes_.size();
  }
  // Returns true if the sheet is successfully added as a render-blocking
  // resource.
  bool AddPendingStylesheet(const Node& owner_node);
  // If the sheet is a render-blocking resource, removes it and returns true;
  // otherwise, returns false with no operation.
  bool RemovePendingStylesheet(const Node& owner_node);

  void AddPendingScript(const ScriptElementBase& script);
  void RemovePendingScript(const ScriptElementBase& script);

  // We additionally allow font preloading (via <link rel="preload"> or Font
  // Loading API) to block rendering for a short period, so that preloaded fonts
  // have a higher chance to be used by the first paint.
  // Design doc: https://bit.ly/36E8UKB
  void FontPreloadingStarted(FontResource*);
  void FontPreloadingFinished(FontResource*, ResourceFinishObserver*);
  void FontPreloadingTimerFired(TimerBase*);
  void ImperativeFontLoadingStarted(FontFace*);
  void ImperativeFontLoadingFinished();

  void Trace(Visitor* visitor) const;

 private:
  friend class RenderBlockingResourceManagerTest;

  // Exposed to unit tests only.
  void SetFontPreloadTimeoutForTest(base::TimeDelta timeout);
  void DisableFontPreloadTimeoutForTest();

  Member<Document> document_;

  // Tracks the currently loading top-level stylesheets which block
  // rendering from starting. Sheets loaded using the @import directive are not
  // directly included in this set. See:
  // https://html.spec.whatwg.org/multipage/links.html#link-type-stylesheet
  // https://html.spec.whatwg.org/multipage/semantics.html#update-a-style-block
  HeapHashSet<Member<const Node>> pending_stylesheet_owner_nodes_;

  // Tracks the currently pending render-blocking script elements.
  HeapHashSet<Member<const ScriptElementBase>> pending_scripts_;

  // Need to hold strong references here, otherwise they'll be GC-ed immediately
  // as Resource only holds weak references.
  HeapHashSet<Member<ResourceFinishObserver>> font_preload_finish_observers_;

  unsigned imperative_font_loading_count_ = 0;

  HeapTaskRunnerTimer<RenderBlockingResourceManager> font_preload_timer_;
  base::TimeDelta font_preload_timeout_;
  bool font_preload_timer_has_fired_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_RENDER_BLOCKING_RESOURCE_MANAGER_H_
