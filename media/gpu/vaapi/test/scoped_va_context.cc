// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/test/scoped_va_context.h"

#include "media/gpu/vaapi/test/macros.h"
#include "media/gpu/vaapi/test/scoped_va_config.h"
#include "media/gpu/vaapi/test/vaapi_device.h"

namespace media {
namespace vaapi_test {

ScopedVAContext::ScopedVAContext(const VaapiDevice& device,
                                 const ScopedVAConfig& config,
                                 const gfx::Size& size)
    : device_(device),
      config_(config),
      context_id_(VA_INVALID_ID),
      size_(size) {
  const VAStatus res =
      vaCreateContext(device_->display(), config_->id(), size_.width(),
                      size_.height(), VA_PROGRESSIVE,
                      /*render_targets=*/nullptr,
                      /*num_render_targets=*/0, &context_id_);
  VA_LOG_ASSERT(res, "vaCreateContext");
  LOG_ASSERT(context_id_ != VA_INVALID_ID)
      << "vaCreateContext created invalid context ID";
  VLOG(1) << "Created context with ID " << context_id_;
}

ScopedVAContext::~ScopedVAContext() {
  VLOG(1) << "Destroying context " << context_id_;
  DCHECK_NE(context_id_, VA_INVALID_ID);
  const VAStatus res = vaDestroyContext(device_->display(), context_id_);
  VA_LOG_ASSERT(res, "vaDestroyContext");
}

}  // namespace vaapi_test
}  // namespace media
