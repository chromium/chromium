// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PARKABLE_IMAGE_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PARKABLE_IMAGE_MANAGER_H_

#include "base/trace_event/memory_dump_provider.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

class DeferredImageDecoder;

// Manages parkable images, which are used in blink::BitmapImage. Currently,
// only records metrics for this. In the future we will park eligible images
// to disk.
// Main Thread only.
class PLATFORM_EXPORT ParkableImageManager
    : public base::trace_event::MemoryDumpProvider {
 public:
  static ParkableImageManager& Instance();
  ~ParkableImageManager() override = default;

  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs&,
                    base::trace_event::ProcessMemoryDump*) override;

 private:
  struct Statistics;

  friend class DeferredImageDecoder;
  friend class base::NoDestructor<ParkableImageManager>;

  ParkableImageManager() = default;

  void Add(DeferredImageDecoder* image);
  void Remove(DeferredImageDecoder* image);

  Statistics ComputeStatistics() const;

  void RecordStatisticsAfter5Minutes() const;

  constexpr static const char* kAllocatorDumpName = "parkable_images";

  WTF::HashSet<DeferredImageDecoder*> image_decoders_;
  bool has_posted_accounting_task_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PARKABLE_IMAGE_MANAGER_H_
