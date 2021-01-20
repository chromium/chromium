// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_FONT_PRELOAD_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_FONT_PRELOAD_MANAGER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/timer.h"

namespace blink {

class Document;
class FontResource;
class FontFace;
class ResourceFinishObserver;

// This class monitors font preloading (via <link rel="preload"> or Font Loading
// API) and notifies the relevant document, so that it can manage the first
// rendering timing to work with preloaded fonts.
// Design doc: https://bit.ly/36E8UKB
class CORE_EXPORT FontPreloadManager final
    : public GarbageCollected<FontPreloadManager> {
 public:
  explicit FontPreloadManager(Document&);
  ~FontPreloadManager() = default;

  FontPreloadManager(const FontPreloadManager&) = delete;
  FontPreloadManager& operator=(const FontPreloadManager&) = delete;

  bool HasPendingRenderBlockingFonts() const;
  void WillBeginRendering();
  bool RenderingHasBegun() const { return state_ == State::kUnblocked; }

  void FontPreloadingStarted(FontResource*);
  void FontPreloadingFinished(FontResource*, ResourceFinishObserver*);
  void FontPreloadingDelaysRenderingTimerFired(TimerBase*);

  void ImperativeFontLoadingStarted(FontFace*);
  void ImperativeFontLoadingFinished();

  // Exposed to web tests via internals.
  void SetRenderDelayTimeoutForTest(base::TimeDelta timeout);

  void Trace(Visitor* visitor) const;

 private:
  friend class FontPreloadManagerTest;

  void DisableTimeoutForTest();

  // State of font preloading before lifecycle updates begin
  enum class State {
    // Rendering hasn't begun. No font preloading yet.
    kInitial,
    // Rendering hasn't begun. There are ongoing font preloadings.
    kLoading,
    // Rendering hasn't begun. At least one font has been preloaded,
    // and all font preloading so far has finished.
    kLoaded,
    // Rendering will begin soon or has begun. Font preloading shouldn't block
    // rendering any more.
    kUnblocked
  };

  void RenderBlockingFontLoadingStarted();
  void RenderBlockingFontLoadingFinished();

  Member<Document> document_;

  // Need to hold strong references here, otherwise they'll be GC-ed immediately
  // as Resource only holds weak references.
  HeapHashSet<Member<ResourceFinishObserver>> finish_observers_;

  unsigned imperative_font_loading_count_ = 0;

  HeapTaskRunnerTimer<FontPreloadManager> render_delay_timer_;
  base::TimeDelta render_delay_timeout_;

  State state_ = State::kInitial;

  // TODO(xiaochengh): Do the same for fonts loaded for other reasons?
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_FONT_PRELOAD_MANAGER_H_
