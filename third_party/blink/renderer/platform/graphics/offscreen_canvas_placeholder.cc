// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/offscreen_canvas_placeholder.h"

#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource.h"
#include "third_party/blink/renderer/platform/graphics/exported_canvas_resource.h"
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

void UpdatePlaceholderClient(
    base::WeakPtr<OffscreenCanvasPlaceholder::Client> client,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    DOMNodeId placeholder_canvas_id) {
  OffscreenCanvasPlaceholder* placeholder_canvas =
      OffscreenCanvasPlaceholder::GetPlaceholderCanvasById(
          placeholder_canvas_id);
  // Note that the placeholder canvas may be destroyed when this post task get
  // to executed.
  if (placeholder_canvas) {
    placeholder_canvas->SetClient(client, task_runner);
  }
}

}  // unnamed namespace

void OffscreenCanvasPlaceholder::Client::UpdatePlaceholderImage(
    base::WeakPtr<OffscreenCanvasPlaceholder::Client> client,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    DOMNodeId placeholder_canvas_id,
    scoped_refptr<blink::ExportedCanvasResource>&& canvas_resource) {
  DCHECK(IsMainThread());

  OffscreenCanvasPlaceholder* placeholder_canvas =
      OffscreenCanvasPlaceholder::GetPlaceholderCanvasById(
          placeholder_canvas_id);
  if (placeholder_canvas) {
    placeholder_canvas->SetOffscreenCanvasResource(std::move(canvas_resource));
    task_runner->PostTask(
        FROM_HERE, base::BindOnce(&Client::OnMainThreadReceivedImage, client));
  }
}

void OffscreenCanvasPlaceholder::Client::DispatchFrame(
    scoped_refptr<ExportedCanvasResource> exported_resource) {
  // Determines whether the main thread may be blocked. If unblocked, post
  // |canvas_resource|. Otherwise, save it but do not post it.
  if (num_pending_placeholder_resources_ < kMaxPendingPlaceholderResources) {
    PostImageToPlaceholder(std::move(exported_resource));
    num_pending_placeholder_resources_++;
  } else {
    DCHECK(num_pending_placeholder_resources_ ==
           kMaxPendingPlaceholderResources);

    latest_unposted_resource_ = std::move(exported_resource);
  }
}

void OffscreenCanvasPlaceholder::Client::PostImageToPlaceholder(
    scoped_refptr<ExportedCanvasResource>&& canvas_resource) {
  // After this point, |canvas_resource| can only be used on the main thread,
  // until it is returned.
  canvas_resource->Transfer();

  CHECK(placeholder_task_runner_);
  PostCrossThreadTask(
      *placeholder_task_runner_, FROM_HERE,
      CrossThreadBindOnce(UpdatePlaceholderImage, GetWeakPtr(),
                          canvas_task_runner_, placeholder_canvas_id_,
                          std::move(canvas_resource)));
}

void OffscreenCanvasPlaceholder::Client::OnMainThreadReceivedImage() {
  num_pending_placeholder_resources_--;

  // The main thread has become unblocked recently and we have a resource that
  // has not been posted yet.
  if (latest_unposted_resource_) {
    DCHECK(num_pending_placeholder_resources_ ==
           kMaxPendingPlaceholderResources - 1);
    DispatchFrame(std::move(latest_unposted_resource_));
    // To make it safe to use/check latest_unposted_resource_ after using
    // std::move on it, we need to force a reset because the move above is
    // elide-able.
    latest_unposted_resource_.reset();
  }
}

void OffscreenCanvasPlaceholder::Client::RegisterWithPlaceholder() {
  // If the offscreencanvas is in the same thread as the canvas, we will update
  // the canvas resource dispatcher directly. So Offscreen Canvas can behave in
  // a more synchronous way when it's on the main thread.
  if (IsMainThread()) {
    UpdatePlaceholderClient(GetWeakPtr(), canvas_task_runner_,
                            placeholder_canvas_id_);
  } else {
    PostCrossThreadTask(
        *placeholder_task_runner_, FROM_HERE,
        CrossThreadBindOnce(UpdatePlaceholderClient, GetWeakPtr(),
                            canvas_task_runner_, placeholder_canvas_id_));
  }
}

void OffscreenCanvasPlaceholder::Client::SetAnimationState(
    OffscreenCanvasPlaceholder::AnimationState animation_state) {
  animation_state_ = animation_state;
  animation_state_callback_.Run();
}

OffscreenCanvasPlaceholder::Client::Client(
    DOMNodeId placeholder_canvas_id,
    scoped_refptr<base::SingleThreadTaskRunner> placeholder_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> canvas_task_runner,
    base::RepeatingClosure animation_state_callback)
    : animation_state_callback_(animation_state_callback),
      placeholder_canvas_id_(placeholder_canvas_id),
      canvas_task_runner_(std::move(canvas_task_runner)),
      placeholder_task_runner_(std::move(placeholder_task_runner)) {
  CHECK(canvas_task_runner_);
  CHECK(placeholder_task_runner_);
  CHECK_NE(placeholder_canvas_id_, kInvalidDOMNodeId);

  RegisterWithPlaceholder();
}

OffscreenCanvasPlaceholder::Client::~Client() = default;

OffscreenCanvasPlaceholder::~OffscreenCanvasPlaceholder() {
  ExportedCanvasResource::OnPlaceholderReleasedResource(
      std::move(placeholder_frame_));
  UnregisterPlaceholderCanvas();
}

void OffscreenCanvasPlaceholder::SetOffscreenCanvasResource(
    scoped_refptr<ExportedCanvasResource>&& new_frame) {
  DCHECK(IsOffscreenCanvasRegistered());
  DCHECK(new_frame);

  ExportedCanvasResource::OnPlaceholderReleasedResource(
      std::move(placeholder_frame_));
  placeholder_frame_ = std::move(new_frame);
}

void OffscreenCanvasPlaceholder::SetClient(
    base::WeakPtr<Client> client,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(IsOffscreenCanvasRegistered());
  client_ = std::move(client);
  client_task_runner_ = std::move(task_runner);

  if (deferred_animation_state_ &&
      current_animation_state_ != *deferred_animation_state_) {
    bool success = PostSetAnimationStateToOffscreenCanvasThread(
        *deferred_animation_state_);
    DCHECK(success);
    current_animation_state_ = *deferred_animation_state_;
    deferred_animation_state_.reset();
  }
}

void OffscreenCanvasPlaceholder::SetSuspendOffscreenCanvasAnimation(
    AnimationState requested_animation_state) {
  if (PostSetAnimationStateToOffscreenCanvasThread(requested_animation_state)) {
    current_animation_state_ = requested_animation_state;
    // If there is any deferred state, clear it because we just posted the
    // correct update.
    deferred_animation_state_.reset();
  } else {
    // Defer the request until we have a client.
    deferred_animation_state_ = requested_animation_state;
  }
}

OffscreenCanvasPlaceholder*
OffscreenCanvasPlaceholder::GetPlaceholderCanvasById(DOMNodeId placeholder_id) {
  CHECK_NE(placeholder_id, kInvalidDOMNodeId);

  PlaceholderIdMap::iterator it = placeholderRegistry().find(placeholder_id);
  if (it == placeholderRegistry().end())
    return nullptr;
  return it->value;
}

void OffscreenCanvasPlaceholder::RegisterPlaceholderCanvas(
    DOMNodeId placeholder_id) {
  CHECK_NE(placeholder_id, kInvalidDOMNodeId);

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
  placeholder_id_ = kInvalidDOMNodeId;
}

bool OffscreenCanvasPlaceholder::PostSetAnimationStateToOffscreenCanvasThread(
    AnimationState animation_state) {
  if (!client_task_runner_) {
    return false;
  }
  PostCrossThreadTask(
      *client_task_runner_, FROM_HERE,
      CrossThreadBindOnce(
          [](base::WeakPtr<OffscreenCanvasPlaceholder::Client> client,
             OffscreenCanvasPlaceholder::AnimationState animation_state) {
            if (client) {
              client->SetAnimationState(animation_state);
            }
          },
          client_, animation_state));
  return true;
}

}  // namespace blink
