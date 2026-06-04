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

void SetAnimationState(
    base::WeakPtr<OffscreenCanvasPlaceholder::Client> client,
    OffscreenCanvasPlaceholder::AnimationState animation_state) {
  if (client) {
    client->SetAnimationState(animation_state);
  }
}

}  // unnamed namespace

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

  if (deferred_animation_state_ &&
      current_animation_state_ != *deferred_animation_state_) {
    bool success = PostSetAnimationStateToOffscreenCanvasThread(
        *deferred_animation_state_);
    DCHECK(success);
    current_animation_state_ = *deferred_animation_state_;
    deferred_animation_state_.reset();
  }
}

void OffscreenCanvasPlaceholder::SetClient(
    base::WeakPtr<Client> client,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(IsOffscreenCanvasRegistered());
  client_ = std::move(client);
  client_task_runner_ = std::move(task_runner);
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
  CHECK_NE(placeholder_id, kNoPlaceholderId);

  PlaceholderIdMap::iterator it = placeholderRegistry().find(placeholder_id);
  if (it == placeholderRegistry().end())
    return nullptr;
  return it->value;
}

void OffscreenCanvasPlaceholder::RegisterPlaceholderCanvas(
    DOMNodeId placeholder_id) {
  CHECK_NE(placeholder_id, kInvalidDOMNodeId);
  CHECK_NE(placeholder_id, kNoPlaceholderId);

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
    AnimationState animation_state) {
  if (!client_task_runner_) {
    return false;
  }
  PostCrossThreadTask(
      *client_task_runner_, FROM_HERE,
      CrossThreadBindOnce(SetAnimationState, client_, animation_state));
  return true;
}

}  // namespace blink
