// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GPU_BROWSER_GPU_CLIENT_DELEGATE_H_
#define CONTENT_BROWSER_GPU_BROWSER_GPU_CLIENT_DELEGATE_H_

#include "components/viz/host/gpu_client_delegate.h"

namespace content {

class BrowserGpuClientDelegate : public viz::GpuClientDelegate {
 public:
  BrowserGpuClientDelegate();

  BrowserGpuClientDelegate(const BrowserGpuClientDelegate&) = delete;
  BrowserGpuClientDelegate& operator=(const BrowserGpuClientDelegate&) = delete;

  ~BrowserGpuClientDelegate() override;

  // GpuClientDelegate:
  viz::GpuHostImpl* EnsureGpuHost() override;
};

}  // namespace content

#endif  // CONTENT_BROWSER_GPU_BROWSER_GPU_CLIENT_DELEGATE_H_
