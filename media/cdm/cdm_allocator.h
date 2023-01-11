// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_CDM_ALLOCATOR_H_
#define MEDIA_CDM_CDM_ALLOCATOR_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/functional/callback.h"
#include "media/base/media_export.h"

namespace cdm {
class Buffer;
}

namespace media {

class VideoFrameImpl;

class MEDIA_EXPORT CdmAllocator {
 public:
  // Callback to create CdmAllocator for the created CDM.
  using CreationCB = base::RepeatingCallback<std::unique_ptr<CdmAllocator>()>;

  CdmAllocator(const CdmAllocator&) = delete;
  CdmAllocator& operator=(const CdmAllocator&) = delete;

  virtual ~CdmAllocator();

  // Creates a buffer with at least |capacity| bytes. Caller is required to
  // call Destroy() on the returned buffer when it is done with it.
  virtual cdm::Buffer* CreateCdmBuffer(size_t capacity) = 0;

  // Returns a new VideoFrameImpl.
  virtual std::unique_ptr<VideoFrameImpl> CreateCdmVideoFrame() = 0;

 protected:
  CdmAllocator();
};

}  // namespace media

#endif  // MEDIA_CDM_CDM_ALLOCATOR_H_
