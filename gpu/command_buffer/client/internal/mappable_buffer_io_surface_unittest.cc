// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/internal/mappable_buffer_io_surface.h"

#include "gpu/command_buffer/client/internal/mappable_buffer_test_template.h"

namespace gpu {
namespace {

INSTANTIATE_TYPED_TEST_SUITE_P(MappableBufferIOSurface,
                               MappableBufferTest,
                               MappableBufferIOSurface);

}  // namespace
}  // namespace gpu
