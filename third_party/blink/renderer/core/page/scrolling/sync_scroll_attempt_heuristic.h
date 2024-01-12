// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_SYNC_SCROLL_ATTEMPT_HEURISTIC_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_SYNC_SCROLL_ATTEMPT_HEURISTIC_H_

#include "base/memory/stack_allocated.h"
#include "third_party/blink/renderer/core/core_export.h"

namespace blink {

class Frame;

// SyncScrollAttemptHeuristic handles detecting cases where a page attempts to
// synchronize an effect with scrolling. While a vended Scope instance exists,
// the heuristic observes when script accesses scroll position and then also
// modifies inline style in order to estimate when script is attempting to
// synchronize content with scrolling. This is complicated somewhat by rAF. It
// can happen that a scroll handler will use rAF to realize the update. Our
// heuristic is imprecise in this case - we consider it to be a sync scroll
// attempt if
//   a) a rAF callback was scheduled via a scroll handler and,
//   b) any rAF callback then mutates inline style or scroll position.
// I.e., we do not distinguish the rAF callback scheduled during the scroll
// handler from any other rAF callback, which can lead to incorrectly flagging a
// rAF-induced mutation as a sync scroll attempt.
//
// We only attempt to detect sync scroll attempts if the given frame is the
// outermost main frame and  UKM is not recorded when the given frame is remote.
//
// TODO(crbug.com/1499981): This should be removed once synchronized scrolling
// impact is understood.
class CORE_EXPORT SyncScrollAttemptHeuristic final {
  STACK_ALLOCATED();

 public:
  explicit SyncScrollAttemptHeuristic(Frame* frame);
  SyncScrollAttemptHeuristic(const SyncScrollAttemptHeuristic&) = delete;
  SyncScrollAttemptHeuristic& operator=(const SyncScrollAttemptHeuristic&) =
      delete;
  ~SyncScrollAttemptHeuristic();

  class Scope {
    STACK_ALLOCATED();

   public:
    explicit Scope(bool enable_observation);
    ~Scope();

   private:
    bool enable_observation_ = false;
  };

  [[nodiscard]] static Scope GetScrollHandlerScope();
  [[nodiscard]] static Scope GetRequestAnimationFrameScope();
  static void DidAccessScrollOffset();
  static void DidSetScrollOffset();
  static void DidSetStyle();
  static void DidRequestAnimationFrame();

 private:
  static void EnableObservation();
  static void DisableObservation();

  Frame* frame_ = nullptr;
  SyncScrollAttemptHeuristic* last_instance_ = nullptr;
  bool did_access_scroll_offset_ = false;
  bool did_set_scroll_offset_ = false;
  bool did_set_style_ = false;
  bool did_request_animation_frame_ = false;
  bool is_observing_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAGE_SCROLLING_SYNC_SCROLL_ATTEMPT_HEURISTIC_H_
