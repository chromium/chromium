// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_OPENXR_UNBOUNDED_SPACE_PROVIDER_H_
#define DEVICE_VR_OPENXR_OPENXR_UNBOUNDED_SPACE_PROVIDER_H_

#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

// A wrapper class to abstract the creation of an unbounded reference space.
class OpenXrUnboundedSpaceProvider {
 public:
  OpenXrUnboundedSpaceProvider();
  virtual ~OpenXrUnboundedSpaceProvider();

  // Returns the type matching the space created by this class.
  virtual XrReferenceSpaceType GetType() const = 0;

  // Create the unbounded space that this type represents for `session`. It will
  // be owned by the caller.
  XrResult CreateSpace(XrSession session, XrSpace* space);
};
}  // namespace device
#endif  // DEVICE_VR_OPENXR_OPENXR_UNBOUNDED_SPACE_PROVIDER_H_
