// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GPU_GPU_FEATURE_CHECKER_IMPL_H_
#define CONTENT_BROWSER_GPU_GPU_FEATURE_CHECKER_IMPL_H_

#include "base/functional/callback.h"
#include "content/public/browser/gpu_data_manager_observer.h"
#include "content/public/browser/gpu_feature_checker.h"
#include "gpu/config/gpu_feature_type.h"

namespace content {

class GpuFeatureCheckerImpl : public GpuFeatureChecker,
                              public GpuDataManagerObserver {
 public:
  GpuFeatureCheckerImpl(gpu::GpuFeatureType feature,
                        FeatureAvailableCallback callback);

  GpuFeatureCheckerImpl(const GpuFeatureCheckerImpl&) = delete;
  GpuFeatureCheckerImpl& operator=(const GpuFeatureCheckerImpl&) = delete;

  // GpuFeatureChecker implementation.
  void CheckGpuFeatureAvailability() override;

  // GpuDataManagerObserver implementation.
  void OnGpuInfoUpdate() override;

 private:
  ~GpuFeatureCheckerImpl() override;

  gpu::GpuFeatureType feature_;
  FeatureAvailableCallback callback_;
  bool checking_ = false;
};

}  // namespace content

#endif  // CONTENT_BROWSER_GPU_GPU_FEATURE_CHECKER_IMPL_H_
