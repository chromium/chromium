// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/gpu_feature_checker_impl.h"

#include "base/check.h"
#include "build/build_config.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/public/browser/browser_thread.h"

namespace content {

// static
scoped_refptr<GpuFeatureChecker> GpuFeatureChecker::Create(
    gpu::GpuFeatureType feature,
    FeatureAvailableCallback callback) {
  return new GpuFeatureCheckerImpl(feature, std::move(callback));
}

GpuFeatureCheckerImpl::GpuFeatureCheckerImpl(gpu::GpuFeatureType feature,
                                             FeatureAvailableCallback callback)
    : feature_(feature), callback_(std::move(callback)) {}

GpuFeatureCheckerImpl::~GpuFeatureCheckerImpl() {}

void GpuFeatureCheckerImpl::CheckGpuFeatureAvailability() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // May only call CheckGpuFeatureAvailability() once.
  DCHECK(!checking_);
  checking_ = true;

  AddRef();  // Matched with a Release in OnGpuInfoUpdate.
  GpuDataManagerImpl* manager = GpuDataManagerImpl::GetInstance();
  manager->AddObserver(this);
  OnGpuInfoUpdate();
}

void GpuFeatureCheckerImpl::OnGpuInfoUpdate() {
  GpuDataManagerImpl* manager = GpuDataManagerImpl::GetInstance();
  if (manager->IsGpuFeatureInfoAvailable()) {
    manager->RemoveObserver(this);
    bool feature_allowed =
        manager->GetFeatureStatus(feature_) == gpu::kGpuFeatureStatusEnabled;
    std::move(callback_).Run(feature_allowed);
    Release();  // Matches the AddRef in CheckGpuFeatureAvailability().
  }
}

}  // namespace content
