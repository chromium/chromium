// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SERVICES_RESOURCE_LIMITS_H_
#define SANDBOX_LINUX_SERVICES_RESOURCE_LIMITS_H_

#include <sys/resource.h>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "sandbox/sandbox_export.h"

namespace sandbox {

// This class provides a small wrapper around setrlimit().
class SANDBOX_EXPORT ResourceLimits {
 public:
  // Lower the soft and hard limit of |resource| to |limit|. If the current
  // limit is lower than |limit|, keep it. Returns 0 on success, or |errno|.
  static int Lower(int resource, rlim_t limit) WARN_UNUSED_RESULT;

  // Lower the soft limit of |resource| to |limit| and the hard limit to |max|.
  // If the current limit is lower than the new values, keep it. Returns 0 on
  // success, or |errno|.
  static int LowerSoftAndHardLimits(int resource,
                                    rlim_t soft_limit,
                                    rlim_t hard_limit) WARN_UNUSED_RESULT;

  // Change soft limit of |resource| to the current limit plus |change|. If
  // |resource + change| is larger than the hard limit, the soft limit is set to
  // be the same as the hard limit. Fails and returns false if the underlying
  // call to setrlimit fails. Returns 0 on success, or |errno|.
  static int AdjustCurrent(int resource,
                           long long int change) WARN_UNUSED_RESULT;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ResourceLimits);
};

}  // namespace sandbox

#endif  // SANDBOX_LINUX_SERVICES_RESOURCE_LIMITS_H_
