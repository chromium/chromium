// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/mock_codec_image.h"

#include "gpu/command_buffer/service/ref_counted_lock_for_test.h"
#include "gpu/config/gpu_finch_features.h"

namespace media {

MockCodecImage::MockCodecImage(const gfx::Size& coded_size)
    : CodecImage(coded_size,
                 features::NeedThreadSafeAndroidMedia()
                     ? base::MakeRefCounted<gpu::RefCountedLockForTest>()
                     : nullptr) {}

MockCodecImage::~MockCodecImage() = default;

}  // namespace media
