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

namespace {

typedef HashMap<int, blink::OffscreenCanvasPlaceholder*> PlaceholderIdMap;

PlaceholderIdMap& placeholderRegistry() {
  DEFINE_STATIC_LOCAL(PlaceholderIdMap, s_placeholderRegistry, ());
  return s_placeholderRegistry;
}

void ReleaseFrameToDispatcher(
    base::WeakPtr<blink::CanvasResourceDispatcher> dispatcher,
    scoped_refptr<blink::CanvasResource> oldImage,
    viz::ResourceId resourceId) {
  if (dispatcher) {
    dispatcher->ReclaimResource(resourceId, std::move(oldImage));
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
    cc::PaintFlags::FilterQuality filter) {
  if (dispatcher) {
    dispatcher->SetFilterQuality(filter);
  }
}

}  // unnamed namespace

namespace blink {

OffscreenCanvasPlaceholder::~OffscreenCanvasPlaceholder() {
  UnregisterPlaceholderCanvas();
}

namespace {

// This function gets called when the last outstanding reference to a
// CanvasResource is released.  This callback is only registered on
// resources received via SetOffscreenCanvasResource(). When the resource
// is received, its ref count may be 2 because the CanvasResourceProvider
// that created it may be holding a cached snapshot that will be released when
// copy-on-write kicks in. This is okay even if the resource provider is on a
// different thread because concurrent read access is safe. By the time the
// next frame is received by OffscreenCanvasPlaceholder, the reference held by
// CanvasResourceProvider will have been released (otherwise there wouldn't be
// a new frame). This means that all outstanding references are held on the
// same thread as the OffscreenCanvasPlaceholder at the time when
// 'placeholder_frame_' is assigned a new value.  Therefore, when the last
// reference is released, we need to temporarily keep the object alive and send
// it back to its thread of origin, where it can be safely destroyed or
// recycled.
void FrameLastUnrefCallback(
    base::WeakPtr<CanvasResourceDispatcher> frame_dispatcher,
    scoped_refptr<base::SingleThreadTaskRunner> frame_dispatcher_task_runner,
    viz::ResourceId placeholder_frame_resource_id,
    scoped_refptr<CanvasResource> placeholder_frame) {
  DCHECK(placeholder_frame);
  DCHECK(placeholder_frame->HasOneRef());
  DCHECK(frame_dispatcher_task_runner);
  placeholder_frame->Transfer();
  PostCrossThreadTask(
      *frame_dispatcher_task_runner, FROM_HERE,
      CrossThreadBindOnce(ReleaseFrameToDispatcher, frame_dispatcher,
                          std::move(placeholder_frame),
                          placeholder_frame_resource_id));
}

}  // unnamed namespace

void OffscreenCanvasPlaceholder::SetOffscreenCanvasResource(
    scoped_refptr<CanvasResource>&& new_frame,
    viz::ResourceId resource_id) {
  DCHECK(IsOffscreenCanvasRegistered());
  DCHECK(new_frame);
  // The following implicitly returns placeholder_frame_ to its
  // CanvasResourceDispatcher, via FrameLastUnrefCallback if it was
  // the last outstanding reference on this thread.
  placeholder_frame_ = std::move(new_frame);
  placeholder_frame_->SetLastUnrefCallback(
      base::BindOnce(FrameLastUnrefCallback, frame_dispatcher_,
                     frame_dispatcher_task_runner_, resource_id));

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
    cc::PaintFlags::FilterQuality quality = filter_quality_.value();
    filter_quality_ = std::nullopt;
    UpdateOffscreenCanvasFilterQuality(quality);
  }
}

void OffscreenCanvasPlaceholder::UpdateOffscreenCanvasFilterQuality(
    cc::PaintFlags::FilterQuality filter_quality) {
  DCHECK(IsOffscreenCanvasRegistered());
  if (!frame_dispatcher_task_runner_) {
    filter_quality_ = filter_quality;
    return;
  }

  if (filter_quality_ == filter_quality)
    return;

  filter_quality_ = filter_quality;
  if (frame_dispatcher_task_runner_->BelongsToCurrentThread()) {
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
      NOTREACHED_IN_MIGRATION();
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
