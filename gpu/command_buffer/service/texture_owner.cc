// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/texture_owner.h"

#include <memory>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "gpu/command_buffer/service/abstract_texture_android.h"
#include "gpu/command_buffer/service/decoder_context.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/image_reader_gl_owner.h"
#include "gpu/command_buffer/service/surface_texture_gl_owner.h"
#include "gpu/command_buffer/service/texture_base.h"
#include "ui/gl/scoped_binders.h"
#include "ui/gl/scoped_make_current.h"

namespace gpu {
namespace {

std::unique_ptr<AbstractTextureAndroid> CreateTexture(
    SharedContextState* context_state) {
  DCHECK(context_state);

  gles2::FeatureInfo* feature_info = context_state->feature_info();
  if (feature_info && feature_info->is_passthrough_cmd_decoder()) {
    return AbstractTextureAndroid::CreateForPassthrough(gfx::Size());
  }

  return AbstractTextureAndroid::CreateForValidating(gfx::Size());
}

}  // namespace

TextureOwner::TextureOwner(bool binds_texture_on_update,
                           std::unique_ptr<AbstractTextureAndroid> texture,
                           scoped_refptr<SharedContextState> context_state)
    : base::RefCountedDeleteOnSequence<TextureOwner>(
          base::SingleThreadTaskRunner::GetCurrentDefault()),
      binds_texture_on_update_(binds_texture_on_update),
      context_state_(std::move(context_state)),
      texture_(std::move(texture)),
      task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()) {
  DCHECK(context_state_);
  context_state_->AddContextLostObserver(this);
}

TextureOwner::TextureOwner(bool binds_texture_on_update,
                           std::unique_ptr<AbstractTextureAndroid> texture)
    : base::RefCountedDeleteOnSequence<TextureOwner>(
          base::SingleThreadTaskRunner::GetCurrentDefault()),
      binds_texture_on_update_(binds_texture_on_update),
      texture_(std::move(texture)),
      task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()) {}

TextureOwner::~TextureOwner() {
  bool have_context = true;
  absl::optional<ui::ScopedMakeCurrent> scoped_make_current;
  if (!context_state_) {
    have_context = false;
  } else {
    if (!context_state_->IsCurrent(nullptr, /*needs_gl=*/true)) {
      scoped_make_current.emplace(context_state_->context(),
                                  context_state_->surface());
      have_context = scoped_make_current->IsContextCurrent();
    }
    context_state_->RemoveContextLostObserver(this);
  }
  if (!have_context)
    texture_->NotifyOnContextLost();

  // Reset texture and context state here while the |context_state_| is current.
  texture_.reset();
  context_state_.reset();
}

// static
scoped_refptr<TextureOwner> TextureOwner::Create(
    Mode mode,
    scoped_refptr<SharedContextState> context_state,
    scoped_refptr<RefCountedLock> drdc_lock) {
  auto texture = CreateTexture(context_state.get());
  switch (mode) {
    case Mode::kAImageReaderInsecure:
    case Mode::kAImageReaderInsecureSurfaceControl:
    case Mode::kAImageReaderSecureSurfaceControl:
      return new ImageReaderGLOwner(std::move(texture), mode,
                                    std::move(context_state),
                                    std::move(drdc_lock));
    case Mode::kSurfaceTextureInsecure:
      DCHECK(!drdc_lock);
      return new SurfaceTextureGLOwner(std::move(texture),
                                       std::move(context_state));
  }

  NOTREACHED();
  return nullptr;
}

GLuint TextureOwner::GetTextureId() const {
  return texture_->service_id();
}

TextureBase* TextureOwner::GetTextureBase() const {
  return texture_->GetTextureBase();
}

void TextureOwner::OnContextLost() {
  ReleaseResources();
  context_state_->RemoveContextLostObserver(this);
  context_state_.reset();
}

}  // namespace gpu
