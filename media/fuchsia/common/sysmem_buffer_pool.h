// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FUCHSIA_COMMON_SYSMEM_BUFFER_POOL_H_
#define MEDIA_FUCHSIA_COMMON_SYSMEM_BUFFER_POOL_H_

#include <fuchsia/media/cpp/fidl.h>
#include <fuchsia/sysmem/cpp/fidl.h>
#include <lib/sys/cpp/component_context.h>

#include <list>
#include <vector>

#include "base/callback.h"
#include "base/containers/span.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"

namespace media {

class SysmemBufferReader;
class SysmemBufferWriter;

// Pool of buffers allocated by sysmem. It owns BufferCollection. It doesn't
// provide any function read/write the buffers. Call should use
// ReadableBufferPool/WritableBufferPool for read/write.
class SysmemBufferPool {
 public:
  using CreateReaderCB =
      base::OnceCallback<void(std::unique_ptr<SysmemBufferReader>)>;
  using CreateWriterCB =
      base::OnceCallback<void(std::unique_ptr<SysmemBufferWriter>)>;

  // Creates SysmemBufferPool asynchronously. It also owns the channel to
  // fuchsia services.
  class Creator {
   public:
    using CreateCB =
        base::OnceCallback<void(std::unique_ptr<SysmemBufferPool>)>;
    Creator(
        fuchsia::sysmem::BufferCollectionPtr collection,
        std::vector<fuchsia::sysmem::BufferCollectionTokenPtr> shared_tokens);
    ~Creator();

    void Create(fuchsia::sysmem::BufferCollectionConstraints constraints,
                CreateCB build_cb);

   private:
    fuchsia::sysmem::BufferCollectionPtr collection_;
    std::vector<fuchsia::sysmem::BufferCollectionTokenPtr> shared_tokens_;
    CreateCB create_cb_;

    THREAD_CHECKER(thread_checker_);

    DISALLOW_COPY_AND_ASSIGN(Creator);
  };

  SysmemBufferPool(
      fuchsia::sysmem::BufferCollectionPtr collection,
      std::vector<fuchsia::sysmem::BufferCollectionTokenPtr> shared_tokens);
  ~SysmemBufferPool();

  fuchsia::sysmem::BufferCollectionTokenPtr TakeToken();

  // Create Reader/Writer to access raw memory. The returned Reader/Writer is
  // owned by SysmemBufferPool and lives as long as SysmemBufferPool.
  void CreateReader(CreateReaderCB create_cb);
  void CreateWriter(CreateWriterCB create_cb);

  // Returns if this object is still usable. Caller must check this before
  // calling SysmemBufferReader/Writer APIs.
  bool is_live() const { return collection_.is_bound(); }

 private:
  friend class BufferAllocator;

  void OnBuffersAllocated(
      zx_status_t status,
      fuchsia::sysmem::BufferCollectionInfo_2 buffer_collection_info);
  void OnError();

  fuchsia::sysmem::BufferCollectionPtr collection_;
  std::vector<fuchsia::sysmem::BufferCollectionTokenPtr> shared_tokens_;

  CreateReaderCB create_reader_cb_;
  CreateWriterCB create_writer_cb_;

  // FIDL interfaces are thread-affine (see crbug.com/1012875).
  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(SysmemBufferPool);
};

// Wrapper of sysmem Allocator.
class BufferAllocator {
 public:
  BufferAllocator();
  ~BufferAllocator();

  std::unique_ptr<SysmemBufferPool::Creator> MakeBufferPoolCreator(
      size_t num_shared_token);

  // TODO(sergeyu): Update FuchsiaVideoDecoder to use SysmemBufferPool and
  // remove this function.
  fuchsia::sysmem::Allocator* raw() { return allocator_.get(); }

 private:
  fuchsia::sysmem::AllocatorPtr allocator_;

  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(BufferAllocator);
};

}  // namespace media

#endif  // MEDIA_FUCHSIA_COMMON_SYSMEM_BUFFER_POOL_H_
