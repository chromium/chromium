// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_FONT_PRELOAD_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_FONT_PRELOAD_MANAGER_H_

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
// TODO(crbug.com/1271296): Rename this class to RenderBlockingResourceManager.
class CORE_EXPORT FontPreloadManager final
    : public GarbageCollected<FontPreloadManager> {
 public:
  explicit FontPreloadManager(Document&);
  ~FontPreloadManager() = default;

  FontPreloadManager(const FontPreloadManager&) = delete;
  FontPreloadManager& operator=(const FontPreloadManager&) = delete;

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
  void FontPreloadingDelaysRenderingTimerFired(TimerBase*);
  void ImperativeFontLoadingStarted(FontFace*);
  void ImperativeFontLoadingFinished();

  void Trace(Visitor* visitor) const;

 private:
  friend class FontPreloadManagerTest;

  bool HasRenderBlockingResources() const {
    return finish_observers_.size() || imperative_font_loading_count_;
  }

  // Exposed to unit tests only.
  void SetRenderDelayTimeoutForTest(base::TimeDelta timeout);
  void DisableTimeoutForTest();

  Member<Document> document_;

  // Need to hold strong references here, otherwise they'll be GC-ed immediately
  // as Resource only holds weak references.
  HeapHashSet<Member<ResourceFinishObserver>> finish_observers_;

  unsigned imperative_font_loading_count_ = 0;

  HeapTaskRunnerTimer<FontPreloadManager> render_delay_timer_;
  base::TimeDelta render_delay_timeout_;
  bool render_delay_timer_has_fired_ = false;

  // https://html.spec.whatwg.org/#awaiting-parser-inserted-body-flag
  // Initialized to true as FontPreloadManager is created only on HTML documents
  bool awaiting_parser_inserted_body_ = true;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_FONT_PRELOAD_MANAGER_H_
