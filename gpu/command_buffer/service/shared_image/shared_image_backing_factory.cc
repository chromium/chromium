// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/shared_image_backing_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"

namespace gpu {

SharedImageBackingFactory::SharedImageBackingFactory() = default;

SharedImageBackingFactory::~SharedImageBackingFactory() = default;

base::WeakPtr<SharedImageBackingFactory>
SharedImageBackingFactory::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void SharedImageBackingFactory::InvalidateWeakPtrsForTesting() {
  weak_ptr_factory_.InvalidateWeakPtrs();
}

}  // namespace gpu
