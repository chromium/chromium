// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/test/fake_libva_driver/fake_context.h"

#include "base/environment.h"
#include "base/notreached.h"
#include "media/gpu/vaapi/test/fake_libva_driver/av1_decoder_delegate.h"
#include "media/gpu/vaapi/test/fake_libva_driver/fake_buffer.h"
#include "media/gpu/vaapi/test/fake_libva_driver/fake_config.h"
#include "media/gpu/vaapi/test/fake_libva_driver/no_op_context_delegate.h"
#include "media/gpu/vaapi/test/fake_libva_driver/vpx_decoder_delegate.h"

namespace {

std::unique_ptr<media::internal::ContextDelegate> CreateDelegate(
    const media::internal::FakeConfig& config,
    int picture_width,
    int picture_height) {
  std::unique_ptr<base::Environment> env = base::Environment::Create();
  CHECK(env);
  std::string no_op_flag;
  if (env->GetVar("USE_NO_OP_CONTEXT_DELEGATE", &no_op_flag) &&
      no_op_flag == "1") {
    return std::make_unique<media::internal::NoOpContextDelegate>();
  }

  if (config.GetEntrypoint() != VAEntrypointVLD) {
    NOTIMPLEMENTED();
    return nullptr;
  }

  switch (config.GetProfile()) {
    case VAProfileVP8Version0_3:
    case VAProfileVP9Profile0:
      return std::make_unique<media::internal::VpxDecoderDelegate>(
          picture_width, picture_height, config.GetProfile());
    case VAProfileAV1Profile0:
      return std::make_unique<media::internal::Av1DecoderDelegate>(
          config.GetProfile());
    default:
      break;
  }

  NOTIMPLEMENTED();
  return nullptr;
}

}  // namespace

namespace media::internal {

FakeContext::FakeContext(FakeContext::IdType id,
                         const FakeConfig& config,
                         int picture_width,
                         int picture_height,
                         int flag,
                         std::vector<VASurfaceID> render_targets)
    : id_(id),
      config_(config),
      picture_width_(picture_width),
      picture_height_(picture_height),
      flag_(flag),
      render_targets_(std::move(render_targets)),
      delegate_(CreateDelegate(*config_, picture_width_, picture_height_)) {}
FakeContext::~FakeContext() = default;

FakeContext::IdType FakeContext::GetID() const {
  return id_;
}

const FakeConfig& FakeContext::GetConfig() const {
  return *config_;
}

int FakeContext::GetPictureWidth() const {
  return picture_width_;
}

int FakeContext::GetPictureHeight() const {
  return picture_height_;
}

int FakeContext::GetFlag() const {
  return flag_;
}

const std::vector<VASurfaceID>& FakeContext::GetRenderTargets() const {
  return render_targets_;
}

void FakeContext::BeginPicture(const FakeSurface& surface) const {
  CHECK(delegate_);
  delegate_->SetRenderTarget(surface);
}

void FakeContext::RenderPicture(
    const std::vector<raw_ptr<const FakeBuffer>>& buffers) const {
  CHECK(delegate_);
  delegate_->EnqueueWork(buffers);
}

void FakeContext::EndPicture() const {
  CHECK(delegate_);
  delegate_->Run();
}

}  // namespace media::internal
