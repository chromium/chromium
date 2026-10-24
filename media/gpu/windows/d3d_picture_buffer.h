// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_D3D_PICTURE_BUFFER_H_
#define MEDIA_GPU_WINDOWS_D3D_PICTURE_BUFFER_H_

#include <concepts>
#include <memory>
#include <vector>

#include "base/check_op.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/ipc/service/command_buffer_stub.h"
#include "media/base/media_log.h"
#include "media/base/video_frame.h"
#include "media/gpu/command_buffer_helper.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/windows/d3d12_fence.h"
#include "media/gpu/windows/d3d_com_defs.h"
#include "media/gpu/windows/d3d_status.h"
#include "media/gpu/windows/d3d_texture_wrapper.h"
#include "ui/gfx/geometry/size.h"

namespace media {

class D3D11PictureBufferImpl;
class D3D12PictureBufferImpl;
class D3DTextureWrapper;
using PictureBufferGPUResourceInitDoneCB =
    base::OnceCallback<void(scoped_refptr<media::D3DPictureBuffer>)>;

// PictureBuffer that owns Chrome Textures to display it, and keeps a reference
// to the D3D11 texture or D3D12 resource that backs the image.
//
// This is created and owned on the decoder thread.  While currently that's the
// gpu main thread, we still keep the decoder parts separate from the chrome GL
// parts, in case that changes.
//
// This is refcounted so that VideoFrame can hold onto it indirectly.  While
// the chrome Texture is sufficient to keep the pictures renderable, we still
// need to guarantee that the client has time to use the mailbox.  Once it
// does so, it would be fine if this were destroyed.  Technically, only the
// GpuResources have to be retained until the mailbox is used, but we just
// retain the whole thing.
class MEDIA_GPU_EXPORT D3DPictureBuffer
    : public base::RefCountedDeleteOnSequence<D3DPictureBuffer> {
 public:
  enum class Type { kD3D11, kD3D12 };

  D3DPictureBuffer(const D3DPictureBuffer&) = delete;
  D3DPictureBuffer& operator=(const D3DPictureBuffer&) = delete;

  // Returns this as T, CHECKing that the implementation matches.
  template <typename T>
    requires(std::same_as<T, D3D11PictureBufferImpl> ||
             std::same_as<T, D3D12PictureBufferImpl>)
  T* As() {
    CHECK_EQ(GetType(), T::kType);
    return static_cast<T*>(this);
  }

  D3DStatus BeginSharedImageAccess() const;

  // Initialize |shared_image_dest|.
  D3DStatus ProcessTexture(
      scoped_refptr<gpu::ClientSharedImage>& shared_image_dest);

  // Opens the D3D11 texture as a D3D12 resource, or returns the cached
  // or native D3D12 resource.
  virtual base::expected<ID3D12Resource*, D3DStatus> GetOrCreateD3D12Resource(
      ID3D12Device* device) = 0;

  void SetFenceAndValue(scoped_refptr<D3D12Fence> fence, uint64_t value);

  size_t picture_index() const { return picture_index_; }

  // Is this PictureBuffer backing a VideoFrame right now?
  bool in_client_use() const { return in_client_use_ > 0; }

  // Is this PictureBuffer holding an image that's in use by the decoder?
  bool in_picture_use() const { return in_picture_use_; }

  void add_client_use() {
    in_client_use_++;
    DCHECK_GT(in_client_use_, 0);
  }
  void remove_client_use() {
    DCHECK_GT(in_client_use_, 0);
    in_client_use_--;
  }
  void set_in_picture_use(bool use) { in_picture_use_ = use; }

  uint32_t array_slice() const { return array_slice_; }
  const gfx::Size& size() const { return size_; }

  D3DTextureWrapper* texture_wrapper() const { return texture_wrapper_.get(); }

  // Shouldn't be here, but simpler for now.
  base::TimeDelta timestamp_;

 protected:
  // |texture_wrapper| controls mailbox access to the backing resource.
  // |array_slice| is the index inside an array texture. |picture_index| is a
  // unique id used to identify this picture to the decoder.
  D3DPictureBuffer(scoped_refptr<base::SequencedTaskRunner> delete_task_runner,
                   size_t array_slice,
                   std::unique_ptr<D3DTextureWrapper> texture_wrapper,
                   const gfx::Size& size,
                   size_t picture_index);
  virtual ~D3DPictureBuffer();

  std::unique_ptr<MediaLog> media_log_;

 private:
  friend class base::RefCountedDeleteOnSequence<D3DPictureBuffer>;
  friend class base::DeleteHelper<D3DPictureBuffer>;

  virtual Type GetType() const = 0;

  uint32_t array_slice_;
  const gfx::Size size_;

  std::unique_ptr<D3DTextureWrapper> texture_wrapper_;

  bool in_picture_use_ = false;
  int in_client_use_ = 0;
  size_t picture_index_;

  // Reused across submissions for this picture buffer. The buffer is not
  // decoded into again until its previous accesses have completed.
  scoped_refptr<gfx::D3DSharedFence> shared_decode_fence_;
};

class MEDIA_GPU_EXPORT D3D11PictureBufferImpl : public D3DPictureBuffer {
 public:
  static constexpr Type kType = Type::kD3D11;

  D3D11PictureBufferImpl(
      scoped_refptr<base::SequencedTaskRunner> delete_task_runner,
      ComD3D11Texture2D texture,
      size_t array_slice,
      std::unique_ptr<D3DTextureWrapper> texture_wrapper,
      const gfx::Size& size,
      size_t picture_index);

  D3DStatus InitD3D11(
      scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
      GetCommandBufferHelperCB get_helper_cb,
      ComD3D11VideoDevice1 video_device,
      const GUID& decoder_guid,
      std::unique_ptr<MediaLog> media_log,
      PictureBufferGPUResourceInitDoneCB
          picture_buffer_gpu_resource_init_done_cb);

  base::expected<ID3D11VideoDecoderOutputView*, D3DStatus> AcquireOutputView()
      const;

  // D3DPictureBuffer:
  base::expected<ID3D12Resource*, D3DStatus> GetOrCreateD3D12Resource(
      ID3D12Device* device) override;

 private:
  // D3DPictureBuffer:
  ~D3D11PictureBufferImpl() override;
  Type GetType() const override;

  ComD3D11Texture2D d3d11_texture_;
  ComD3D11VideoDecoderOutputView output_view_;

  // The cached D3D12 version of the texture.
  ComD3D12Resource d3d12_resource_;
};

class MEDIA_GPU_EXPORT D3D12PictureBufferImpl : public D3DPictureBuffer {
 public:
  static constexpr Type kType = Type::kD3D12;

  D3D12PictureBufferImpl(
      scoped_refptr<base::SequencedTaskRunner> delete_task_runner,
      ComD3D12Resource resource,
      uint32_t array_slice,
      std::unique_ptr<D3DTextureWrapper> texture_wrapper,
      const gfx::Size& size,
      size_t picture_index);

  // D3DPictureBuffer:
  base::expected<ID3D12Resource*, D3DStatus> GetOrCreateD3D12Resource(
      ID3D12Device* device) override;

 private:
  // D3DPictureBuffer:
  ~D3D12PictureBufferImpl() override;
  Type GetType() const override;

  ComD3D12Resource d3d12_resource_;
};

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_D3D_PICTURE_BUFFER_H_
