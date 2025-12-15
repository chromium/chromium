/*
 * Copyright (c) 2006,2007,2008, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"

#include "partition_alloc/partition_alloc.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"
#include "third_party/skia/modules/skcms/skcms.h"

namespace blink {

bool ApproximatelyEqualSkColorSpaces(sk_sp<SkColorSpace> src_color_space,
                                     sk_sp<SkColorSpace> dst_color_space) {
  if ((!src_color_space && dst_color_space) ||
      (src_color_space && !dst_color_space))
    return false;
  if (!src_color_space && !dst_color_space)
    return true;
  skcms_ICCProfile src_profile, dst_profile;
  src_color_space->toProfile(&src_profile);
  dst_color_space->toProfile(&dst_profile);
  return skcms_ApproximatelyEqualProfiles(&src_profile, &dst_profile);
}

sk_sp<SkData> TryAllocateSkData(size_t size) {
  void* buffer =
      Partitions::BufferPartition()
          ->AllocInline<partition_alloc::AllocFlags::kReturnNull |
                        partition_alloc::AllocFlags::kZeroFill>(size, "SkData");
  if (!buffer)
    return nullptr;
  return SkData::MakeWithProc(
      buffer, size,
      [](const void* buffer, void* context) {
        Partitions::BufferPartition()->Free(const_cast<void*>(buffer));
      },
      /*context=*/nullptr);
}

}  // namespace blink
