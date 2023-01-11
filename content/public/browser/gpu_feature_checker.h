// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_GPU_FEATURE_CHECKER_H_
#define CONTENT_PUBLIC_BROWSER_GPU_FEATURE_CHECKER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "gpu/config/gpu_feature_type.h"

namespace content {

// GpuFeatureChecker does an asynchronous lookup for a GpuFeatureType. It will
// keep itself alive (via being a refcounted object) until it calls the given
// |callback|. The feature check is not initiated (and a self-reference is not
// taken) until CheckGpuFeatureAvailability() is called. If the feature is
// already available, the |callback| may be called synchronously inside of the
// CheckGpuFeatureAvailability() call.
class GpuFeatureChecker : public base::RefCountedThreadSafe<GpuFeatureChecker> {
 public:
  using FeatureAvailableCallback =
      base::OnceCallback<void(bool feature_available)>;

  CONTENT_EXPORT static scoped_refptr<GpuFeatureChecker> Create(
      gpu::GpuFeatureType feature,
      FeatureAvailableCallback callback);

  // Checks to see if |feature_| is available on the current GPU. |callback_|
  // will be called to indicate the availability of the feature. It may be
  // called synchronously if the data is already available, or else will be
  // called asynchronously when it becomes available. This method must be called
  // from the the UI thread.
  // CheckGpuFeatureAvailability() may be called at most once for any given
  // GpuFeatureChecker
  virtual void CheckGpuFeatureAvailability() = 0;

 protected:
  friend class base::RefCountedThreadSafe<GpuFeatureChecker>;

  virtual ~GpuFeatureChecker();
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_GPU_FEATURE_CHECKER_H_
