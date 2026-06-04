// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_OFFSCREEN_CANVAS_PLACEHOLDER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_OFFSCREEN_CANVAS_PLACEHOLDER_H_

#include <optional>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "cc/paint/paint_flags.h"
#include "third_party/blink/renderer/platform/graphics/dom_node_id.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class ExportedCanvasResource;

class PLATFORM_EXPORT OffscreenCanvasPlaceholder {
  DISALLOW_NEW();

 public:
  enum {
    kNoPlaceholderId = -1,
  };

  enum class AnimationState {
    // Animation should be active, and use the real sync signal from viz.
    kActive,

    // Animation should be active, but should use a synthetic sync signal.  This
    // is useful when viz won't provide us with one.
    kActiveWithSyntheticTiming,

    // Animation should be suspended.
    kSuspended,
  };

  class Client {
   public:
    virtual ~Client();
    virtual void SetAnimationState(AnimationState animation_state) = 0;
  };

  ~OffscreenCanvasPlaceholder();

  virtual void SetOffscreenCanvasResource(
      scoped_refptr<ExportedCanvasResource>&&);
  void SetClient(base::WeakPtr<Client>,
                 scoped_refptr<base::SingleThreadTaskRunner>);

  void SetSuspendOffscreenCanvasAnimation(AnimationState requested_state);

  static OffscreenCanvasPlaceholder* GetPlaceholderCanvasById(
      DOMNodeId placeholder_id);

  void RegisterPlaceholderCanvas(DOMNodeId placeholder_id);
  void UnregisterPlaceholderCanvas();
  bool HasOffscreenCanvasFrame() const { return !!placeholder_frame_; }
  ExportedCanvasResource* OffscreenCanvasFrame() const {
    return placeholder_frame_.get();
  }

  bool IsOffscreenCanvasRegistered() const {
    return placeholder_id_ != kNoPlaceholderId;
  }

  virtual bool HasCanvasCapture() const { return false; }

  AnimationState GetAnimationStateForTesting() const {
    return current_animation_state_;
  }

 private:
  bool PostSetAnimationStateToOffscreenCanvasThread(
      AnimationState animation_state);

  // Information about the Offscreen Canvas:
  scoped_refptr<ExportedCanvasResource> placeholder_frame_;
  base::WeakPtr<Client> client_;
  scoped_refptr<base::SingleThreadTaskRunner> client_task_runner_;

  DOMNodeId placeholder_id_ = kNoPlaceholderId;

  // If an animation state change was requested, but we couldn't update it
  // immediately, then this holds the most recent request.
  std::optional<AnimationState> deferred_animation_state_;

  // Most recent animation state sent to the client.
  AnimationState current_animation_state_ = AnimationState::kActive;

  std::optional<cc::PaintFlags::FilterQuality> filter_quality_ = std::nullopt;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_OFFSCREEN_CANVAS_PLACEHOLDER_H_
