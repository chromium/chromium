// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_CONFIG_GPU_DOMAIN_GUILT_H_
#define GPU_CONFIG_GPU_DOMAIN_GUILT_H_

namespace gpu {

// Indicates the guilt level of a domain which caused a GPU reset. If a domain
// is 100% known to be guilty of resetting the GPU, then it will generally not
// cause other domains' use of 3D APIs to be blocked, unless system stability
// would be compromised.
enum class DomainGuilt {
  kKnown,
  kUnknown,
};

}  // namespace gpu

#endif  // GPU_CONFIG_GPU_DOMAIN_GUILT_H_
