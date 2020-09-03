/*
 * Copyright (C) 2004, 2008, 2009, 2010 Apple Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_FRAME_CARET_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_FRAME_CARET_H_

#include <memory>
#include "base/single_thread_task_runner.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/editing/forward.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/graphics/paint_invalidation_reason.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/timer.h"

namespace blink {

class CaretDisplayItemClient;
class FrameCaret;
class GraphicsContext;
class LayoutBlock;
class LocalFrame;
class SelectionEditor;
struct PaintInvalidatorContext;
struct PhysicalOffset;

class CORE_EXPORT FrameCaret final : public GarbageCollected<FrameCaret> {
 public:
  FrameCaret(LocalFrame&, const SelectionEditor&);
  ~FrameCaret();

  bool IsActive() const;

  void ScheduleVisualUpdateForPaintInvalidationIfNeeded();

  // Used to suspend caret blinking while the mouse is down.
  void SetCaretBlinkingSuspended(bool suspended) {
    is_caret_blinking_suspended_ = suspended;
  }
  bool IsCaretBlinkingSuspended() const { return is_caret_blinking_suspended_; }
  void StopCaretBlinkTimer();
  void StartBlinkCaret();
  void SetCaretEnabled(bool);
  IntRect AbsoluteCaretBounds() const;

  bool ShouldShowBlockCursor() const { return should_show_block_cursor_; }
  void SetShouldShowBlockCursor(bool);

  // Paint invalidation methods delegating to DisplayItemClient.
  void LayoutBlockWillBeDestroyed(const LayoutBlock&);
  void UpdateStyleAndLayoutIfNeeded();
  void InvalidatePaint(const LayoutBlock&, const PaintInvalidatorContext&);

  bool ShouldPaintCaret(const LayoutBlock&) const;
  void PaintCaret(GraphicsContext&, const PhysicalOffset&) const;

  // For unit tests.
  const CaretDisplayItemClient& CaretDisplayItemClientForTesting() const {
    return *display_item_client_;
  }
  bool IsVisibleIfActiveForTesting() const;
  void RecreateCaretBlinkTimerForTesting(
      scoped_refptr<base::SingleThreadTaskRunner>);

  void Trace(Visitor*) const;

 private:
  friend class FrameCaretTest;
  friend class FrameSelectionTest;

  const PositionWithAffinity CaretPosition() const;

  bool ShouldShowCaret() const;
  void CaretBlinkTimerFired(TimerBase*);
  void UpdateAppearance();

  const Member<const SelectionEditor> selection_editor_;
  const Member<LocalFrame> frame_;
  const std::unique_ptr<CaretDisplayItemClient> display_item_client_;
  // TODO(https://crbug.com/668758): Consider using BeginFrame update for this.
  std::unique_ptr<TaskRunnerTimer<FrameCaret>> caret_blink_timer_;
  bool is_caret_enabled_ = false;
  bool should_show_caret_ = false;
  bool is_caret_blinking_suspended_ = false;
  bool should_show_block_cursor_ = false;

  DISALLOW_COPY_AND_ASSIGN(FrameCaret);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_FRAME_CARET_H_
