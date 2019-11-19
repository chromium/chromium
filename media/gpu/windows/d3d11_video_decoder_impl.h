// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_D3D11_VIDEO_DECODER_IMPL_H_
#define MEDIA_GPU_D3D11_VIDEO_DECODER_IMPL_H_

#include <d3d11_1.h>
#include <wrl/client.h>

#include <list>
#include <memory>
#include <string>
#include <tuple>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "media/gpu/command_buffer_helper.h"
#include "media/gpu/media_gpu_export.h"

namespace gpu {
struct SyncToken;
}  // namespace gpu

namespace media {

class MediaLog;
class D3D11PictureBuffer;

// Does the gpu main thread work for D3D11VideoDecoder.  Except as noted, this
// class lives on the GPU main thread.
// TODO(liberato): Rename this class as a follow-on to this refactor.
class MEDIA_GPU_EXPORT D3D11VideoDecoderImpl {
 public:
  // May be constructed on any thread.
  explicit D3D11VideoDecoderImpl(
      std::unique_ptr<MediaLog> media_log,
      base::RepeatingCallback<scoped_refptr<CommandBufferHelper>()>
          get_helper_cb);
  virtual ~D3D11VideoDecoderImpl();

  using InitCB = base::OnceCallback<void(bool success)>;

  // Returns a picture buffer that's no longer in use by the client.
  using ReturnPictureBufferCB =
      base::RepeatingCallback<void(scoped_refptr<D3D11PictureBuffer>)>;

  // We will call back |init_cb| with the init status.  |try_decoding_cb| should
  // try to re-start decoding.  We'll call this when we do something that might
  // allow decoding to make progress, such as reclaim a picture buffer.
  virtual void Initialize(InitCB init_cb,
                          ReturnPictureBufferCB return_picture_buffer_cb);

  // Called when the VideoFrame that uses |buffer| is freed.
  void OnMailboxReleased(scoped_refptr<D3D11PictureBuffer> buffer,
                         const gpu::SyncToken& sync_token);

  // Return a weak ptr, since D3D11VideoDecoder constructs callbacks for us.
  // May be called from any thread.
  base::WeakPtr<D3D11VideoDecoderImpl> GetWeakPtr();

 private:
  void OnSyncTokenReleased(scoped_refptr<D3D11PictureBuffer> buffer);

  std::unique_ptr<MediaLog> media_log_;

  // Called when we get a picture buffer back from the client.
  ReturnPictureBufferCB return_picture_buffer_cb_;

  base::RepeatingCallback<scoped_refptr<CommandBufferHelper>()> get_helper_cb_;
  scoped_refptr<CommandBufferHelper> helper_;

  // Has thread affinity -- must be run on the gpu main thread.
  THREAD_CHECKER(thread_checker_);

  base::WeakPtrFactory<D3D11VideoDecoderImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(D3D11VideoDecoderImpl);
};

}  // namespace media

#endif  // MEDIA_GPU_D3D11_VIDEO_DECODER_IMPL_H_
