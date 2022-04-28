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
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_preferences.h"
#include "media/base/bitrate.h"
#include "media/base/video_codecs.h"
#include "media/base/win/dxgi_device_manager.h"
#include "media/gpu/media_gpu_export.h"
#include "media/video/h264_parser.h"
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
  explicit MediaFoundationVideoEncodeAccelerator(
      const gpu::GpuPreferences& gpu_preferences,
      const gpu::GpuDriverBugWorkarounds& gpu_workarounds);

  MediaFoundationVideoEncodeAccelerator(
      const MediaFoundationVideoEncodeAccelerator&) = delete;
  MediaFoundationVideoEncodeAccelerator& operator=(
      const MediaFoundationVideoEncodeAccelerator&) = delete;

  // VideoEncodeAccelerator implementation.
  VideoEncodeAccelerator::SupportedProfiles GetSupportedProfiles() override;
  VideoEncodeAccelerator::SupportedProfiles GetSupportedProfilesLight()
      override;
  bool Initialize(const Config& config,
                  Client* client,
                  std::unique_ptr<MediaLog> media_log) override;
  void Encode(scoped_refptr<VideoFrame> frame, bool force_keyframe) override;
  void UseOutputBitstreamBuffer(BitstreamBuffer buffer) override;
  void RequestEncodingParametersChange(const Bitrate& bitrate,
                                       uint32_t framerate) override;
  void Destroy() override;
  bool IsGpuFrameResizeSupported() override;

  // Preloads dlls required for encoding. Returns true if all required dlls are
  // correctly loaded.
  static bool PreSandboxInitialization();

  enum class DriverVendor { kOther, kNvidia, kIntel, kAMD };

 protected:
  ~MediaFoundationVideoEncodeAccelerator() override;

 private:
  // Holds output buffers coming from the client ready to be filled.
  struct BitstreamBufferRef;

  // Holds output buffers coming from the encoder.
  class EncodeOutput;

  // Get supported profiles for specific codec.
  VideoEncodeAccelerator::SupportedProfiles GetSupportedProfilesForCodec(
      VideoCodec codec,
      bool populate_svc_info);

  // Enumerates all hardware encoder backed IMFTransform instances for given
  // codec.
  uint32_t EnumerateHardwareEncoders(VideoCodec codec,
                                     IMFActivate*** pp_activate);

  // Activates the asynchronous encoder instance |encoder_| according to codec
  // merit.
  bool ActivateAsyncEncoder(IMFActivate** pp_activate,
                            uint32_t activate_count,
                            bool is_constrained_h264);

  // Initializes and allocates memory for input and output parameters.
  bool InitializeInputOutputParameters(VideoCodecProfile output_profile,
                                       bool is_constrained_h264);

  // Initializes encoder parameters for real-time use.
  bool SetEncoderModes();

  // Helper function to notify the client of an error on
  // |main_client_task_runner_|.
  void NotifyError(VideoEncodeAccelerator::Error error);

  // Encoding task to be run on |encoder_thread_task_runner_|.
  void EncodeTask(scoped_refptr<VideoFrame> frame, bool force_keyframe);

  // Processes the input video frame for the encoder.
  HRESULT ProcessInput(scoped_refptr<VideoFrame> frame, bool force_keyframe);

  // Populates input sample buffer with contents of a video frame
  HRESULT PopulateInputSampleBuffer(scoped_refptr<VideoFrame> frame);
  HRESULT PopulateInputSampleBufferGpu(scoped_refptr<VideoFrame> frame);

  // Assign TemporalID by bitstream or external state machine(based on SVC
  // Spec).
  bool AssignTemporalId(Microsoft::WRL::ComPtr<IMFMediaBuffer> output_buffer,
                        size_t size,
                        int* temporal_id,
                        bool keyframe);

  int AssignTemporalIdBySvcSpec(bool keyframe);

  bool temporalScalableCoding() { return num_temporal_layers_ > 1; }

  // Checks for and copies encoded output on |encoder_thread_task_runner_|.
  void ProcessOutput();

  // Drains pending output samples on |encoder_thread_task_runner_|.
  void DrainPendingOutputs();

  // Tries to deliver the input frame to the encoder.
  bool TryToDeliverInputFrame(scoped_refptr<VideoFrame> frame,
                              bool force_keyframe);

  // Tries to return a bitstream buffer to the client.
  void TryToReturnBitstreamBuffer();

  // Inserts the output buffers for reuse on |encoder_thread_task_runner_|.
  void UseOutputBitstreamBufferTask(
      std::unique_ptr<BitstreamBufferRef> buffer_ref);

  // Changes encode parameters on |encoder_thread_task_runner_|.
  void RequestEncodingParametersChangeTask(const Bitrate& bitrate,
                                           uint32_t framerate);

  // Destroys encode session on |encoder_thread_task_runner_|.
  void DestroyTask();

  // Releases resources encoder holds.
  void ReleaseEncoderResources();

  // Initialize video processing (for scaling)
  HRESULT InitializeD3DVideoProcessing(ID3D11Texture2D* input_texture);

  // Perform D3D11 scaling operation
  HRESULT PerformD3DScaling(ID3D11Texture2D* input_texture);

  const bool compatible_with_win7_;
  const bool disable_dynamic_framerate_update_;

  // Bitstream buffers ready to be used to return encoded output as a FIFO.
  base::circular_deque<std::unique_ptr<BitstreamBufferRef>>
      bitstream_buffer_queue_;

  // EncodeOutput needs to be copied into a BitstreamBufferRef as a FIFO.
  base::circular_deque<std::unique_ptr<EncodeOutput>> encoder_output_queue_;

  // Counter of outputs which is used to assign temporal layer indexes
  // according to the corresponding layer pattern. Reset for every key frame.
  uint32_t outputs_since_keyframe_count_ = 0;

  // This parser is used to assign temporalId.
  H264Parser h264_parser_;

  gfx::Size input_visible_size_;
  size_t bitstream_buffer_size_;
  uint32_t frame_rate_;
  Bitrate bitrate_;
  bool low_latency_mode_;
  int num_temporal_layers_ = 1;

  // Codec type used for encoding.
  VideoCodec codec_ = VideoCodec::kUnknown;

  // Vendor of the active video encoder.
  DriverVendor vendor_ = DriverVendor::kOther;

  // Group of picture length for encoded output stream, indicates the
  // distance between two key frames.
  uint32_t gop_length_;

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
  Microsoft::WRL::ComPtr<ID3D11VideoProcessor> video_processor_;
  Microsoft::WRL::ComPtr<ID3D11VideoProcessorEnumerator>
      video_processor_enumerator_;
  Microsoft::WRL::ComPtr<ID3D11VideoDevice> video_device_;
  Microsoft::WRL::ComPtr<ID3D11VideoContext> video_context_;
  D3D11_VIDEO_PROCESSOR_CONTENT_DESC vp_desc_ = {};
  Microsoft::WRL::ComPtr<ID3D11Texture2D> scaled_d3d11_texture_;
  Microsoft::WRL::ComPtr<ID3D11VideoProcessorOutputView> vp_output_view_;

  // To expose client callbacks from VideoEncodeAccelerator.
  // NOTE: all calls to this object *MUST* be executed on
  // |main_client_task_runner_|.
  base::WeakPtr<Client> main_client_;
  std::unique_ptr<base::WeakPtrFactory<Client>> main_client_weak_factory_;
  scoped_refptr<base::SequencedTaskRunner> main_client_task_runner_;
  SEQUENCE_CHECKER(sequence_checker_);

  // This thread services tasks posted from the VEA API entry points
  // and runs them on a thread that can do heavy work and call MF COM interface.
  scoped_refptr<base::SingleThreadTaskRunner> encoder_thread_task_runner_;

  // DXGI device manager for handling hardware input textures
  scoped_refptr<DXGIDeviceManager> dxgi_device_manager_;

  // A buffer used as a scratch space for I420 to NV12 conversion
  std::vector<uint8_t> resize_buffer_;

  // Declared last to ensure that all weak pointers are invalidated before
  // other destructors run.
  base::WeakPtr<MediaFoundationVideoEncodeAccelerator> encoder_weak_ptr_;
  base::WeakPtrFactory<MediaFoundationVideoEncodeAccelerator>
      encoder_task_weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_MEDIA_FOUNDATION_VIDEO_ENCODE_ACCELERATOR_WIN_H_
