// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_TEST_SCOPED_VA_CONTEXT_H_
#define MEDIA_GPU_VAAPI_TEST_SCOPED_VA_CONTEXT_H_

#include <va/va.h>

#include "base/memory/raw_ref.h"
#include "ui/gfx/geometry/size.h"

namespace media {
namespace vaapi_test {

class ScopedVAConfig;
class VaapiDevice;

// Provides a wrapper around a VAContext that properly handles creation and
// destruction. Decoders should use this to recreate a context when the VAConfig
// or size changes.
// The associated VaapiDevice and ScopedVAConfig must be guaranteed externally
// to be alive beyond the lifetime of the ScopedVAContext.
class ScopedVAContext {
 public:
  // Constructs a VAContext with given |size|. Requires an initialized |device|.
  // Success is ASSERTed.
  ScopedVAContext(const VaapiDevice& device,
                  const ScopedVAConfig& config,
                  const gfx::Size& size);

  ScopedVAContext(const ScopedVAContext&) = delete;
  ScopedVAContext& operator=(const ScopedVAContext&) = delete;
  ~ScopedVAContext();

  VAContextID id() const { return context_id_; }
  const gfx::Size& size() const { return size_; }

 private:
  // Non-owned.
  const raw_ref<const VaapiDevice> device_;
  const raw_ref<const ScopedVAConfig> config_;

  VAContextID context_id_;
  const gfx::Size size_;
};

}  // namespace vaapi_test
}  // namespace media

#endif  // MEDIA_GPU_VAAPI_TEST_SCOPED_VA_CONTEXT_H_
