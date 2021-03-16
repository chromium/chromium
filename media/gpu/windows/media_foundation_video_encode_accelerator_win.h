// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_MEDIA_FOUNDATION_VIDEO_ENCODE_ACCELERATOR_WIN_H_
#define MEDIA_GPU_WINDOWS_MEDIA_FOUNDATION_VIDEO_ENCODE_ACCELERATOR_WIN_H_

#include <mfapi.h>
#include <mfidl.h>
#include <stdint.h>
#include <strmif.h>
#include <wrl/client.h>

#include <memory>

#include "base/bind.h"
#include "base/containers/circular_deque.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "media/base/win/dxgi_device_manager.h"
#include "media/base/win/mf_initializer.h"
#include "media/gpu/media_gpu_export.h"
#include "media/video/video_encode_accelerator.h"

namespace media {

// Media Foundation implementation of the VideoEncodeAccelerator interface for
// Windows.
// This class saves the task runner on which it is constructed and runs client
// callbacks using that same task runner.
// This class has DCHECKs to makes sure that methods are called in the
// correct task runners. It starts an internal encoder thread on which
// VideoEncodeAccelerator implementation tasks are posted.
class MEDIA_GPU_EXPORT MediaFoundationVideoEncodeAccelerator
    : public VideoEncodeAccelerator {
 public:
  // If |compatible_with_win7| is true, MediaFoundationVideoEncoderAccelerator
  // works on Windows 7. Some attributes of the encoder are not supported on old
  // systems, which may impact the performance or quality of the output.
  explicit MediaFoundationVideoEncodeAccelerator(bool compatible_with_win7,
                                                 bool enable_async_mft);

  // VideoEncodeAccelerator implementation.
  VideoEncodeAccelerator::SupportedProfiles GetSupportedProfiles() override;
  bool Initialize(const Config& config, Client* client) override;
  void Encode(scoped_refptr<VideoFrame> frame, bool force_keyframe) override;
  void UseOutputBitstreamBuffer(BitstreamBuffer buffer) override;
  void RequestEncodingParametersChange(uint32_t bitrate,
                                       uint32_t framerate) override;
  void Destroy() override;

  // Preloads dlls required for encoding. Returns true if all required dlls are
  // correctly loaded.
  static bool PreSandboxInitialization();

 protected:
  ~MediaFoundationVideoEncodeAccelerator() override;

 private:
  // Holds output buffers coming from the client ready to be filled.
  struct BitstreamBufferRef;

  // Holds output buffers coming from the encoder.
  class EncodeOutput;

  // Enumerates all hardware encoder backed IMFTransform instances.
  uint32_t EnumerateHardwareEncoders(IMFActivate*** pp_activate);

  // Activates the asynchronous encoder instance |encoder_| according to codec
  // merit.
  bool ActivateAsyncEncoder(IMFActivate** pp_activate, uint32_t activate_count);

  // Initializes and allocates memory for input and output parameters.
  bool InitializeInputOutputParameters(VideoCodecProfile output_profile,
                                       bool is_constrained_h264);

  // Initializes encoder parameters for real-time use.
  bool SetEncoderModes();

  // Helper function to notify the client of an error on
  // |main_client_task_runner_|.
  void NotifyError(VideoEncodeAccelerator::Error error);

  // Encoding tasks to be run on |encoder_thread_|.
  void EncodeTask(scoped_refptr<VideoFrame> frame, bool force_keyframe);
  void AsyncEncodeTask(scoped_refptr<VideoFrame> frame, bool force_keyframe);
  void SyncEncodeTask(scoped_refptr<VideoFrame> frame, bool force_keyframe);

  // Processes the input video frame for the encoder.
  HRESULT ProcessInput(scoped_refptr<VideoFrame> frame, bool force_keyframe);

  // Populates input sample buffer with contents of a video frame
  HRESULT PopulateInputSampleBuffer(scoped_refptr<VideoFrame> frame);

  // Checks for and copies encoded output on |encoder_thread_|.
  void ProcessOutputAsync();
  void ProcessOutputSync();

  // Tries to deliver the input frame to the encoder.
  bool TryToDeliverInputFrame(scoped_refptr<VideoFrame> frame,
                              bool force_keyframe);

  // Tries to return a bitstream buffer to the client.
  void TryToReturnBitstreamBuffer();

  // Inserts the output buffers for reuse on |encoder_thread_|.
  void UseOutputBitstreamBufferTask(
      std::unique_ptr<BitstreamBufferRef> buffer_ref);

  // Changes encode parameters on |encoder_thread_|.
  void RequestEncodingParametersChangeTask(uint32_t bitrate,
                                           uint32_t framerate);

  // Destroys encode session on |encoder_thread_|.
  void DestroyTask();

  // Releases resources encoder holds.
  void ReleaseEncoderResources();

  const bool compatible_with_win7_;

  // Flag to enable the usage of MFTEnumEx.
  const bool enable_async_mft_;

  // Whether asynchronous hardware encoder enabled or not.
  bool is_async_mft_;

  // Bitstream buffers ready to be used to return encoded output as a FIFO.
  base::circular_deque<std::unique_ptr<BitstreamBufferRef>>
      bitstream_buffer_queue_;

  // EncodeOutput needs to be copied into a BitstreamBufferRef as a FIFO.
  base::circular_deque<std::unique_ptr<EncodeOutput>> encoder_output_queue_;

  gfx::Size input_visible_size_;
  size_t bitstream_buffer_size_;
  uint32_t frame_rate_;
  uint32_t target_bitrate_;

  Microsoft::WRL::ComPtr<IMFActivate> activate_;
  Microsoft::WRL::ComPtr<IMFTransform> encoder_;
  Microsoft::WRL::ComPtr<ICodecAPI> codec_api_;
  Microsoft::WRL::ComPtr<IMFMediaEventGenerator> event_generator_;

  DWORD input_stream_id_;
  DWORD output_stream_id_;

  Microsoft::WRL::ComPtr<IMFMediaType> imf_input_media_type_;
  Microsoft::WRL::ComPtr<IMFMediaType> imf_output_media_type_;

  bool input_required_;
  Microsoft::WRL::ComPtr<IMFSample> input_sample_;
  Microsoft::WRL::ComPtr<IMFSample> output_sample_;

  // MediaFoundation session.
  MFSessionLifetime session_;

  // To expose client callbacks from VideoEncodeAccelerator.
  // NOTE: all calls to this object *MUST* be executed on
  // |main_client_task_runner_|.
  base::WeakPtr<Client> main_client_;
  std::unique_ptr<base::WeakPtrFactory<Client>> main_client_weak_factory_;
  scoped_refptr<base::SingleThreadTaskRunner> main_client_task_runner_;

  // This thread services tasks posted from the VEA API entry points by the
  // GPU child thread and CompressionCallback() posted from device thread.
  base::Thread encoder_thread_;
  scoped_refptr<base::SingleThreadTaskRunner> encoder_thread_task_runner_;

  // DXGI device manager for handling hardware input textures
  scoped_refptr<DXGIDeviceManager> dxgi_device_manager_;

  // Declared last to ensure that all weak pointers are invalidated before
  // other destructors run.
  base::WeakPtrFactory<MediaFoundationVideoEncodeAccelerator>
      encoder_task_weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MediaFoundationVideoEncodeAccelerator);
};

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_MEDIA_FOUNDATION_VIDEO_ENCODE_ACCELERATOR_WIN_H_
