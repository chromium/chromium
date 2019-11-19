// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/offscreen_canvas_placeholder.h"

#include "base/single_thread_task_runner.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_dispatcher.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace {

typedef HashMap<int, blink::OffscreenCanvasPlaceholder*> PlaceholderIdMap;

PlaceholderIdMap& placeholderRegistry() {
  DEFINE_STATIC_LOCAL(PlaceholderIdMap, s_placeholderRegistry, ());
  return s_placeholderRegistry;
}

void releaseFrameToDispatcher(
    base::WeakPtr<blink::CanvasResourceDispatcher> dispatcher,
    scoped_refptr<blink::CanvasResource> oldImage,
    viz::ResourceId resourceId) {
  oldImage = nullptr;  // Needed to unref'ed on the right thread
  if (dispatcher) {
    dispatcher->ReclaimResource(resourceId);
  }
}

void SetSuspendAnimation(
    base::WeakPtr<blink::CanvasResourceDispatcher> dispatcher,
    bool suspend) {
  if (dispatcher) {
    dispatcher->SetSuspendAnimation(suspend);
  }
}

void UpdateDispatcherFilterQuality(
    base::WeakPtr<blink::CanvasResourceDispatcher> dispatcher,
    SkFilterQuality filter) {
  if (dispatcher) {
    dispatcher->SetFilterQuality(filter);
  }
}

}  // unnamed namespace

namespace blink {

OffscreenCanvasPlaceholder::~OffscreenCanvasPlaceholder() {
  UnregisterPlaceholderCanvas();
}

void OffscreenCanvasPlaceholder::SetOffscreenCanvasResource(
    scoped_refptr<CanvasResource> new_frame,
    viz::ResourceId resource_id) {
  DCHECK(IsOffscreenCanvasRegistered());
  DCHECK(new_frame);
  ReleaseOffscreenCanvasFrame();
  placeholder_frame_ = std::move(new_frame);
  placeholder_frame_resource_id_ = resource_id;

  if (animation_state_ == kShouldSuspendAnimation) {
    bool success = PostSetSuspendAnimationToOffscreenCanvasThread(true);
    DCHECK(success);
    animation_state_ = kSuspendedAnimation;
  } else if (animation_state_ == kShouldActivateAnimation) {
    bool success = PostSetSuspendAnimationToOffscreenCanvasThread(false);
    DCHECK(success);
    animation_state_ = kActiveAnimation;
  }
}

void OffscreenCanvasPlaceholder::SetOffscreenCanvasDispatcher(
    base::WeakPtr<CanvasResourceDispatcher> dispatcher,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(IsOffscreenCanvasRegistered());
  frame_dispatcher_ = std::move(dispatcher);
  frame_dispatcher_task_runner_ = std::move(task_runner);
  // The UpdateOffscreenCanvasFilterQuality could be called to change the filter
  // quality before this function. We need to first apply the filter changes to
  // the corresponding offscreen canvas.
  if (filter_quality_) {
    SkFilterQuality quality = filter_quality_.value();
    filter_quality_ = base::nullopt;
    UpdateOffscreenCanvasFilterQuality(quality);
  }
}

void OffscreenCanvasPlaceholder::ReleaseOffscreenCanvasFrame() {
  DCHECK(IsOffscreenCanvasRegistered());
  if (!placeholder_frame_)
    return;

  DCHECK(frame_dispatcher_task_runner_);
  placeholder_frame_->Transfer();
  PostCrossThreadTask(
      *frame_dispatcher_task_runner_, FROM_HERE,
      CrossThreadBindOnce(releaseFrameToDispatcher, frame_dispatcher_,
                          std::move(placeholder_frame_),
                          placeholder_frame_resource_id_));
}

void OffscreenCanvasPlaceholder::UpdateOffscreenCanvasFilterQuality(
    SkFilterQuality filter_quality) {
  DCHECK(IsOffscreenCanvasRegistered());
  if (!frame_dispatcher_task_runner_) {
    filter_quality_ = filter_quality;
    return;
  }

  if (filter_quality_ == filter_quality)
    return;

  filter_quality_ = filter_quality;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      Thread::Current()->GetTaskRunner();
  if (task_runner == frame_dispatcher_task_runner_) {
    UpdateDispatcherFilterQuality(frame_dispatcher_, filter_quality);
  } else {
    PostCrossThreadTask(*frame_dispatcher_task_runner_, FROM_HERE,
                        CrossThreadBindOnce(UpdateDispatcherFilterQuality,
                                            frame_dispatcher_, filter_quality));
  }
}

void OffscreenCanvasPlaceholder::SetSuspendOffscreenCanvasAnimation(
    bool suspend) {
  switch (animation_state_) {
    case kActiveAnimation:
      if (suspend) {
        if (PostSetSuspendAnimationToOffscreenCanvasThread(suspend)) {
          animation_state_ = kSuspendedAnimation;
        } else {
          animation_state_ = kShouldSuspendAnimation;
        }
      }
      break;
    case kSuspendedAnimation:
      if (!suspend) {
        if (PostSetSuspendAnimationToOffscreenCanvasThread(suspend)) {
          animation_state_ = kActiveAnimation;
        } else {
          animation_state_ = kShouldActivateAnimation;
        }
      }
      break;
    case kShouldSuspendAnimation:
      if (!suspend) {
        animation_state_ = kActiveAnimation;
      }
      break;
    case kShouldActivateAnimation:
      if (suspend) {
        animation_state_ = kSuspendedAnimation;
      }
      break;
    default:
      NOTREACHED();
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

bool OffscreenCanvasPlaceholder::PostSetSuspendAnimationToOffscreenCanvasThread(
    bool suspend) {
  if (!frame_dispatcher_task_runner_)
    return false;
  PostCrossThreadTask(
      *frame_dispatcher_task_runner_, FROM_HERE,
      CrossThreadBindOnce(SetSuspendAnimation, frame_dispatcher_, suspend));
  return true;
}

}  // namespace blink
