// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/wrapped_graphite_texture_holder.h"

#include "gpu/command_buffer/service/shared_context_state.h"
#include "third_party/skia/include/gpu/graphite/Context.h"

namespace gpu {

WrappedGraphiteTextureHolder::WrappedGraphiteTextureHolder(
    skgpu::graphite::BackendTexture backend_texture,
    scoped_refptr<SharedContextState> context_state,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : texture_(std::move(backend_texture)),
      context_state_(std::move(context_state)),
      task_runner_(std::move(task_runner)) {}

WrappedGraphiteTextureHolder::~WrappedGraphiteTextureHolder() {
  auto destroy_resource = [](scoped_refptr<SharedContextState> context_state,
                             skgpu::graphite::BackendTexture texture) {
    if (texture.isValid()) {
      context_state->graphite_context()->deleteBackendTexture(texture);
    }
  };

  if (task_runner_ && !task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(destroy_resource, std::move(context_state_), texture_));
  } else {
    destroy_resource(std::move(context_state_), texture_);
  }
}

}  // namespace gpu
