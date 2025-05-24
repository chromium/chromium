// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/offscreen_canvas_placeholder.h"

#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_dispatcher.h"
#include "third_party/blink/renderer/platform/graphics/resource_id_traits.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {
namespace {

typedef HashMap<int, OffscreenCanvasPlaceholder*> PlaceholderIdMap;

PlaceholderIdMap& placeholderRegistry() {
  DEFINE_STATIC_LOCAL(PlaceholderIdMap, s_placeholderRegistry, ());
  return s_placeholderRegistry;
}

void SetAnimationState(
    base::WeakPtr<CanvasResourceDispatcher> dispatcher,
    CanvasResourceDispatcher::AnimationState animation_state) {
  if (dispatcher) {
    dispatcher->SetAnimationState(animation_state);
  }
}

}  // unnamed namespace

OffscreenCanvasPlaceholder::~OffscreenCanvasPlaceholder() {
  UnregisterPlaceholderCanvas();
}

namespace {

}  // unnamed namespace

void OffscreenCanvasPlaceholder::SetOffscreenCanvasResource(
    scoped_refptr<CanvasResource>&& new_frame,
    viz::ResourceId resource_id) {
  DCHECK(IsOffscreenCanvasRegistered());
  DCHECK(new_frame);
  CanvasResource::OnPlaceholderReleasedResource(std::move(placeholder_frame_));
  placeholder_frame_ = std::move(new_frame);

  if (deferred_animation_state_ &&
      current_animation_state_ != *deferred_animation_state_) {
    bool success = PostSetAnimationStateToOffscreenCanvasThread(
        *deferred_animation_state_);
    DCHECK(success);
    current_animation_state_ = *deferred_animation_state_;
    deferred_animation_state_.reset();
  }
}

void OffscreenCanvasPlaceholder::SetOffscreenCanvasDispatcher(
    base::WeakPtr<CanvasResourceDispatcher> dispatcher,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(IsOffscreenCanvasRegistered());
  frame_dispatcher_ = std::move(dispatcher);
  frame_dispatcher_task_runner_ = std::move(task_runner);
}

void OffscreenCanvasPlaceholder::SetSuspendOffscreenCanvasAnimation(
    CanvasResourceDispatcher::AnimationState requested_animation_state) {
  if (PostSetAnimationStateToOffscreenCanvasThread(requested_animation_state)) {
    current_animation_state_ = requested_animation_state;
    // If there is any deferred state, clear it because we just posted the
    // correct update.
    deferred_animation_state_.reset();
  } else {
    // Defer the request until we have a dispatcher.
    deferred_animation_state_ = requested_animation_state;
  }
}

OffscreenCanvasPlaceholder*
OffscreenCanvasPlaceholder::GetPlaceholderCanvasById(unsigned placeholder_id) {
  PlaceholderIdMap::iterator it = placeholderRegistry().find(placeholder_id);
  if (it == placeholderRegistry().end())
    return nullptr;
  return it->value;
}

void OffscreenCanvasPlaceholder::RegisterPlaceholderCanvas(
    unsigned placeholder_id) {
  DCHECK(!placeholderRegistry().Contains(placeholder_id));
  DCHECK(!IsOffscreenCanvasRegistered());
  placeholderRegistry().insert(placeholder_id, this);
  placeholder_id_ = placeholder_id;
}

void OffscreenCanvasPlaceholder::UnregisterPlaceholderCanvas() {
  if (!IsOffscreenCanvasRegistered())
    return;
  DCHECK(placeholderRegistry().find(placeholder_id_)->value == this);
  placeholderRegistry().erase(placeholder_id_);
  placeholder_id_ = kNoPlaceholderId;
}

bool OffscreenCanvasPlaceholder::PostSetAnimationStateToOffscreenCanvasThread(
    CanvasResourceDispatcher::AnimationState animation_state) {
  if (!frame_dispatcher_task_runner_)
    return false;
  PostCrossThreadTask(*frame_dispatcher_task_runner_, FROM_HERE,
                      CrossThreadBindOnce(SetAnimationState, frame_dispatcher_,
                                          animation_state));
  return true;
}

}  // namespace blink
