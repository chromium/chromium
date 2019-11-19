// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_CDM_ALLOCATOR_H_
#define MEDIA_MOJO_SERVICES_MOJO_CDM_ALLOCATOR_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "media/cdm/cdm_allocator.h"
#include "media/mojo/services/media_mojo_export.h"
#include "mojo/public/cpp/system/buffer.h"

namespace media {

// This is a CdmAllocator that creates buffers using mojo shared memory.
class MEDIA_MOJO_EXPORT MojoCdmAllocator : public CdmAllocator {
 public:
  MojoCdmAllocator();
  ~MojoCdmAllocator() final;

  // CdmAllocator implementation.
  cdm::Buffer* CreateCdmBuffer(size_t capacity) final;
  std::unique_ptr<VideoFrameImpl> CreateCdmVideoFrame() final;

 private:
  friend class MojoCdmAllocatorTest;

  // Map of available buffers. Done as a mapping of capacity to
  // ScopedSharedBufferHandle so that we can efficiently find an available
  // buffer of a particular size. Any buffers in the map are unmapped.
  using AvailableBufferMap =
      std::multimap<size_t, mojo::ScopedSharedBufferHandle>;

  // Allocates a mojo::SharedBufferHandle of at least |capacity| bytes.
  // |capacity| will be changed to reflect the actual size of the buffer
  // allocated.
  mojo::ScopedSharedBufferHandle AllocateNewBuffer(size_t* capacity);

  // Returns |buffer| to the map of available buffers, ready to be used the
  // next time CreateCdmBuffer() is called.
  void AddBufferToAvailableMap(mojo::ScopedSharedBufferHandle buffer,
                               size_t capacity);

  // Returns the MojoHandle for a cdm::Buffer allocated by this class.
  MojoHandle GetHandleForTesting(cdm::Buffer* buffer);

  // Returns the number of buffers in |available_buffers_|.
  size_t GetAvailableBufferCountForTesting();

  // Map of available, already allocated buffers.
  AvailableBufferMap available_buffers_;

  // Confirms single-threaded access.
  base::ThreadChecker thread_checker_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<MojoCdmAllocator> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MojoCdmAllocator);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_CDM_ALLOCATOR_H_
