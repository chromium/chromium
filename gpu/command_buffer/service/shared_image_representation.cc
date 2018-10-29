// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_representation.h"

namespace gpu {

SharedImageRepresentation::SharedImageRepresentation(
    SharedImageManager* manager,
    SharedImageBacking* backing)
    : manager_(manager), backing_(backing) {}
SharedImageRepresentation::~SharedImageRepresentation() {
  manager_->OnRepresentationDestroyed(backing_->mailbox());
}

}  // namespace gpu
