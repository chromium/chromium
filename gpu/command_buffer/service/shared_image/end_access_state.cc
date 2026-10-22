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
    std::unique_ptr<skgpu::MutableTextureState> end_state)
    : context_(context),
      texture_(std::move(texture)),
      end_state_(std::move(end_state)) {
  CHECK(context_);
  CHECK(texture_.isValid());
  CHECK(end_state_->isValid());
}

EndAccessState::EndAccessState(EndAccessState&& other)
    : context_(std::exchange(other.context_, nullptr)),
      texture_(std::exchange(other.texture_, {})),
      end_state_(std::move(other.end_state_)) {}

EndAccessState& EndAccessState::operator=(EndAccessState&& other) {
  if (this != &other) {
    CHECK(!NeedsApply());
    context_ = std::exchange(other.context_, nullptr);
    texture_ = std::exchange(other.texture_, {});
    end_state_ = std::move(other.end_state_);
  }
  return *this;
}

EndAccessState::~EndAccessState() {
  CHECK(!NeedsApply());
}

bool EndAccessState::NeedsApply() const {
  return context_ != nullptr;
}

void EndAccessState::Apply() {
  if (!NeedsApply()) {
    return;
  }
  if (!context_->setMutableState(texture_, *end_state_)) {
    LOG(ERROR) << "setMutableState() failed.";
  }
  context_ = nullptr;
}

}  // namespace gpu
