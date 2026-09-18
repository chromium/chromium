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
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace gfx {
class RectF;
}

namespace blink {

class ExportedCanvasResource;

class PLATFORM_EXPORT OffscreenCanvasPlaceholder {
  DISALLOW_NEW();

 public:
  enum class AnimationState {
    // Animation should be active, and use the real sync signal from viz.
    kActive,

    // Animation should be active, but should use a synthetic sync signal.  This
    // is useful when viz won't provide us with one.
    kActiveWithSyntheticTiming,

    // Animation should be suspended.
    kSuspended,
  };

  class PLATFORM_EXPORT Client {
   public:
    // We set a limit to the number of placeholder resources that have been
    // posted to the main thread but not yet received on that thread.
    static constexpr unsigned kMaxPendingPlaceholderResources = 50;

    Client(DOMNodeId placeholder_canvas_id,
           scoped_refptr<base::SingleThreadTaskRunner> placeholder_task_runner,
           scoped_refptr<base::SingleThreadTaskRunner> offscreen_canvas_runner,
           base::RepeatingClosure animation_state_callback);
    virtual ~Client();

    void DispatchFrame(scoped_refptr<ExportedCanvasResource>);
    OffscreenCanvasPlaceholder::AnimationState GetAnimationState() {
      return animation_state_;
    }

   protected:
    // virtual and protected for testing
    virtual void PostImageToPlaceholder(
        scoped_refptr<ExportedCanvasResource>&&);
    // virtual for mocking
    virtual void OnMainThreadReceivedImage();

   private:
    friend class OffscreenCanvasPlaceholderTest;
    friend class OffscreenCanvasPlaceholder;

    base::WeakPtr<Client> GetWeakPtr() {
      return weak_ptr_factory_.GetWeakPtr();
    }

    void SetAnimationState(
        OffscreenCanvasPlaceholder::AnimationState animation_state);

    void RegisterWithPlaceholder();

    static void UpdatePlaceholderImage(
        base::WeakPtr<Client> client,
        scoped_refptr<base::SingleThreadTaskRunner> task_runner,
        DOMNodeId placeholder_canvas_id,
        scoped_refptr<blink::ExportedCanvasResource>&& canvas_resource);

    base::RepeatingClosure animation_state_callback_;

    const DOMNodeId placeholder_canvas_id_;

    // Task runner for the thread where OffscreenCanvas lives.
    scoped_refptr<base::SingleThreadTaskRunner> canvas_task_runner_;
    // Task runner for the thread where OffscreenCanvasPlaceholder lives, i.e
    // main thread.
    scoped_refptr<base::SingleThreadTaskRunner> placeholder_task_runner_;

    OffscreenCanvasPlaceholder::AnimationState animation_state_ =
        OffscreenCanvasPlaceholder::AnimationState::kActive;

    // The latest_unposted_resource_ always refers to the frame
    // resource used by the latest_unposted_resource_.
    scoped_refptr<ExportedCanvasResource> latest_unposted_resource_;
    unsigned num_pending_placeholder_resources_ = 0;

    base::WeakPtrFactory<Client> weak_ptr_factory_{this};
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
    return placeholder_id_ != kInvalidDOMNodeId;
  }

  virtual bool HasCanvasCapture() const { return false; }

  virtual void RecordRenderedText(const String& text,
                                  const gfx::RectF& bounds,
                                  float font_height) {}
  virtual void ClearRenderedText(const gfx::RectF& rect) {}
  virtual void ClearRenderedText() {}

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

  DOMNodeId placeholder_id_ = kInvalidDOMNodeId;

  // If an animation state change was requested, but we couldn't update it
  // immediately, then this holds the most recent request.
  std::optional<AnimationState> deferred_animation_state_;

  // Most recent animation state sent to the client.
  AnimationState current_animation_state_ = AnimationState::kActive;

  std::optional<cc::PaintFlags::FilterQuality> filter_quality_ = std::nullopt;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_OFFSCREEN_CANVAS_PLACEHOLDER_H_
