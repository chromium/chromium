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
class ResourceFinishObserver;

// https://html.spec.whatwg.org/#render-blocking-mechanism with some extensions.
class CORE_EXPORT RenderBlockingResourceManager final
    : public GarbageCollected<RenderBlockingResourceManager> {
 public:
  explicit RenderBlockingResourceManager(Document&);
  ~RenderBlockingResourceManager() = default;

  RenderBlockingResourceManager(const RenderBlockingResourceManager&) = delete;
  RenderBlockingResourceManager& operator=(
      const RenderBlockingResourceManager&) = delete;

  void WillInsertBody() { awaiting_parser_inserted_body_ = false; }

  // https://html.spec.whatwg.org/#render-blocked
  bool IsRenderBlocked() const {
    return awaiting_parser_inserted_body_ || HasRenderBlockingResources();
  }

  // TODO(crbug.com/1271296): Use this class to handle render-blocking scripts,
  // stylesheets and preloads.

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

  bool HasRenderBlockingResources() const {
    return font_preload_finish_observers_.size() ||
           imperative_font_loading_count_;
  }

  // Exposed to unit tests only.
  void SetFontPreloadTimeoutForTest(base::TimeDelta timeout);
  void DisableFontPreloadTimeoutForTest();

  Member<Document> document_;

  // Need to hold strong references here, otherwise they'll be GC-ed immediately
  // as Resource only holds weak references.
  HeapHashSet<Member<ResourceFinishObserver>> font_preload_finish_observers_;

  unsigned imperative_font_loading_count_ = 0;

  HeapTaskRunnerTimer<RenderBlockingResourceManager> font_preload_timer_;
  base::TimeDelta font_preload_timeout_;
  bool font_preload_timer_has_fired_ = false;

  // https://html.spec.whatwg.org/#awaiting-parser-inserted-body-flag
  // Initialized to true as RenderBlockingResourceManager is created only on
  // HTML documents
  bool awaiting_parser_inserted_body_ = true;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_RENDER_BLOCKING_RESOURCE_MANAGER_H_
