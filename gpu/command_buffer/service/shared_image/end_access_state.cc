// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/end_access_state.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/logging.h"
#include "gpu/command_buffer/service/graphite_shared_context.h"
#include "third_party/skia/include/gpu/MutableTextureState.h"

namespace gpu {

EndAccessState::EndAccessState() = default;

EndAccessState::EndAccessState(
    GraphiteSharedContext* context,
    skgpu::graphite::BackendTexture texture,
    std::unique_ptr<skgpu::MutableTextureState> texture_state,
    const skgpu::graphite::BackendSemaphore& signal_semaphore)
    : context_(context),
      texture_(std::move(texture)),
      texture_state_(std::move(texture_state)),
      signal_semaphore_(signal_semaphore) {
  DCHECK(context_);
  DCHECK(texture_.isValid());
  DCHECK(texture_state_->isValid());
  DCHECK(signal_semaphore_->isValid());
}

EndAccessState::EndAccessState(EndAccessState&& other)
    : context_(std::exchange(other.context_, nullptr)),
      texture_(std::exchange(other.texture_, {})),
      texture_state_(std::move(other.texture_state_)),
      signal_semaphore_(std::exchange(other.signal_semaphore_, {})),
      signal_generation_(std::exchange(other.signal_generation_, {})) {}

EndAccessState& EndAccessState::operator=(EndAccessState&& other) {
  if (this != &other) {
    CHECK(!NeedsApply());
    context_ = std::exchange(other.context_, nullptr);
    texture_ = std::exchange(other.texture_, {});
    texture_state_ = std::move(other.texture_state_);
    signal_semaphore_ = std::exchange(other.signal_semaphore_, {});
    signal_generation_ = std::exchange(other.signal_generation_, {});
  }
  return *this;
}

EndAccessState::~EndAccessState() {
  CHECK(!NeedsApply());

  if (signal_generation_) {
    // The signal semaphore must be submitted before the representation exports
    // a sync fd from the semaphore. If the caller didn't call submit, trigger
    // it here before end access logic runs and export fails.
    if (context_->submit_generation() == *signal_generation_) {
      LOG(ERROR) << "Access was ended without submit";
      context_->submit();
    }
  }
}

bool EndAccessState::NeedsApply() const {
  return texture_state_ != nullptr;
}

void EndAccessState::Apply() {
  if (!NeedsApply()) {
    return;
  }
  if (!context_->setMutableState(texture_, *texture_state_)) {
    LOG(ERROR) << "setMutableState() failed.";
  }
  texture_state_.reset();
  signal_generation_ = context_->AddPendingSignalSemaphore(*signal_semaphore_);
  signal_semaphore_.reset();
}

}  // namespace gpu
