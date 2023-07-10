// Copyright 2016 The Chromium Authors
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

#include "base/atomic_ref_count.h"
#include "base/containers/circular_deque.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/win/shlwapi.h"
#include "base/win/windows_types.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_preferences.h"
#include "media/base/bitrate.h"
#include "media/base/video_codecs.h"
#include "media/base/video_encoder.h"
#include "media/base/win/dxgi_device_manager.h"
#include "media/gpu/media_gpu_export.h"
#include "media/video/h264_parser.h"
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
#include "media/video/h265_nalu_parser.h"
#endif
#include "media/video/video_encode_accelerator.h"

namespace media {

class VideoRateControlWrapper;

// Media Foundation implementation of the VideoEncodeAccelerator interface for
// Windows.
// This class saves the task runner on which it is constructed and runs client
// callbacks using that same task runner.
// This class has DCHECKs to makes sure that methods are called in the
// correct task runners. It starts an internal encoder thread on which
// VideoEncodeAccelerator implementation tasks are posted.
class MEDIA_GPU_EXPORT MediaFoundationVideoEncodeAccelerator
    : public VideoEncodeAccelerator,
      public IMFAsyncCallback {
 public:
  explicit MediaFoundationVideoEncodeAccelerator(
      const gpu::GpuPreferences& gpu_preferences,
      const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
      CHROME_LUID luid);

  MediaFoundationVideoEncodeAccelerator(
      const MediaFoundationVideoEncodeAccelerator&) = delete;
  MediaFoundationVideoEncodeAccelerator& operator=(
      const MediaFoundationVideoEncodeAccelerator&) = delete;

  // VideoEncodeAccelerator implementation.
  VideoEncodeAccelerator::SupportedProfiles GetSupportedProfiles() override;
  bool Initialize(const Config& config,
                  Client* client,
                  std::unique_ptr<MediaLog> media_log) override;
  void Encode(scoped_refptr<VideoFrame> frame, bool force_keyframe) override;
  void UseOutputBitstreamBuffer(BitstreamBuffer buffer) override;
  void RequestEncodingParametersChange(const Bitrate& bitrate,
                                       uint32_t framerate) override;
  void RequestEncodingParametersChange(
      const VideoBitrateAllocation& bitrate_allocation,
      uint32_t framerate) override;
  void Destroy() override;
  void Flush(FlushCallback flush_callback) override;
  bool IsFlushSupported() override;
  bool IsGpuFrameResizeSupported() override;

  // IMFAsyncCallback implementation
  IFACEMETHODIMP GetParameters(DWORD* pdwFlags, DWORD* pdwQueue) override;
  IFACEMETHODIMP Invoke(IMFAsyncResult* pAsyncResult) override;
  IFACEMETHODIMP_(ULONG) AddRef() override;
  IFACEMETHODIMP_(ULONG) Release() override;
  IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override;

  enum class DriverVendor { kOther, kNvidia, kIntel, kAMD };

 protected:
  ~MediaFoundationVideoEncodeAccelerator() override;

 private:
  // Holds output buffers coming from the client ready to be filled.
  struct BitstreamBufferRef;

  // Holds output buffers coming from the encoder.
  class EncodeOutput;

  // Pending encode input.
  using PendingInput = VideoEncoder::PendingEncode;

  // Encoder state.
  enum State {
    kUninitialized,
    kInitializing,
    kEncoding,
    // We wait to feed all pending frames from `pending_input_queue_`
    // before telling MF encoder to drain.
    kPreFlushing,
    // We issued a drain message to the MF encoder want wait for the drain
    // to complete.
    kFlushing,
    kError,
  };

  // Get supported profiles for specific codec.
  VideoEncodeAccelerator::SupportedProfiles GetSupportedProfilesForCodec(
      VideoCodec codec);

  // Activates the asynchronous encoder instance |encoder_| according to codec
  // merit.
  bool ActivateAsyncEncoder(IMFActivate** pp_activates,
                            uint32_t activate_count,
                            bool is_constrained_h264);

  // Initializes and allocates memory for input and output parameters.
  bool InitializeInputOutputParameters(VideoCodecProfile output_profile,
                                       bool is_constrained_h264);

  // Initializes encoder parameters for real-time use.
  bool SetEncoderModes();

  // Helper function to notify the client of an error status. This also sets
  // the state to kError.
  void NotifyErrorStatus(EncoderStatus status);

  // Set the encoder state to |state|.
  void SetState(State state);

  // Processes the input video frame for the encoder.
  HRESULT ProcessInput(const PendingInput& input);

  // Feed as many frames from |pending_input_queue_| to ProcessInput()
  // as possible.
  void FeedInputs();

  // Populates input sample buffer with contents of a video frame
  HRESULT PopulateInputSampleBuffer(const PendingInput& input);
  HRESULT PopulateInputSampleBufferGpu(scoped_refptr<VideoFrame> frame);
  HRESULT CopyInputSampleBufferFromGpu(const VideoFrame& frame);

  // Assign TemporalID by bitstream or external state machine(based on SVC
  // Spec).
  bool AssignTemporalId(Microsoft::WRL::ComPtr<IMFMediaBuffer> output_buffer,
                        size_t size,
                        int* temporal_id,
                        bool keyframe);

  int AssignTemporalIdBySvcSpec(bool keyframe);

  bool temporal_scalable_coding() const { return num_temporal_layers_ > 1; }

  // Checks for and copies encoded output.
  void ProcessOutput();

  // Asynchronous event handler
  void MediaEventHandler(MediaEventType event_type, HRESULT status);

  // Sends MFT_MESSAGE_COMMAND_DRAIN to the encoder to make it
  // process all inputs, produce all outputs and tell us when it's done.
  void DrainEncoder();

  // Initialize video processing (for scaling).
  HRESULT InitializeD3DVideoProcessing(ID3D11Texture2D* input_texture);
  // Scales `input_texture` to size `input_visible_size_`. On success, the
  // result is stored in `scaled_d3d11_texture_`.
  HRESULT PerformD3DScaling(ID3D11Texture2D* input_texture);

  // Initializes the video copying operation by making sure
  // `copied_d3d11_texture_` exists and that its size matches `input_texture`.
  HRESULT InitializeD3DCopying(ID3D11Texture2D* input_texture);
  // Copies `input_texture` to `copied_d3d11_texture_`.
  HRESULT PerformD3DCopy(ID3D11Texture2D* input_texture);

  // Used to post tasks from the IMFMediaEvent::Invoke() method.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  std::unique_ptr<MediaLog> media_log_;

  // Bitstream buffers ready to be used to return encoded output as a FIFO.
  base::circular_deque<std::unique_ptr<BitstreamBufferRef>>
      bitstream_buffer_queue_;

  // Input frame queue for encoding on next METransformNeedInput event.
  base::circular_deque<PendingInput> pending_input_queue_;

  // EncodeOutput needs to be copied into a BitstreamBufferRef as a FIFO.
  base::circular_deque<std::unique_ptr<EncodeOutput>> encoder_output_queue_;

  // Counter of outputs which is used to assign temporal layer indexes
  // according to the corresponding layer pattern. Reset for every key frame.
  uint32_t outputs_since_keyframe_count_ = 0;

  // Encoder state. Encode tasks will only run in kEncoding state.
  State state_ = kUninitialized;

  // This parser is used to assign temporalId.
  H264Parser h264_parser_;
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
  H265NaluParser h265_nalu_parser_;
#endif

  gfx::Size input_visible_size_;
  size_t bitstream_buffer_size_ = 0u;
  uint32_t frame_rate_ = 30;
  // For recording configured frame rate as we don't dynamically change it.
  // The default value here will be overridden during initialization.
  uint32_t configured_frame_rate_ = 30;
  // Bitrate allocation in bps.
  VideoBitrateAllocation bitrate_allocation_{Bitrate::Mode::kConstant};
  bool low_latency_mode_ = false;
  int num_temporal_layers_ = 1;

  // Codec type used for encoding.
  VideoCodec codec_ = VideoCodec::kUnknown;

  // Vendor of the active video encoder.
  DriverVendor vendor_ = DriverVendor::kOther;

  // Group of picture length for encoded output stream, indicates the
  // distance between two key frames.
  uint32_t gop_length_ = 0u;

  // Video encoder info that includes accelerator name, QP validity, etc.
  VideoEncoderInfo encoder_info_;
  bool encoder_info_sent_ = false;

  Microsoft::WRL::ComPtr<IMFActivate> activate_;
  Microsoft::WRL::ComPtr<IMFTransform> encoder_;
  Microsoft::WRL::ComPtr<ICodecAPI> codec_api_;
  Microsoft::WRL::ComPtr<IMFMediaEventGenerator> event_generator_;
  base::AtomicRefCount async_callback_ref_{1};

  DWORD input_stream_id_ = 0u;
  DWORD output_stream_id_ = 0u;

  Microsoft::WRL::ComPtr<IMFMediaType> imf_input_media_type_;
  Microsoft::WRL::ComPtr<IMFMediaType> imf_output_media_type_;

  Microsoft::WRL::ComPtr<IMFSample> input_sample_;
  // True if `input_sample_` has been populated with data/metadata
  // of the next frame to be encoded.
  bool has_prepared_input_sample_ = false;

  // Variables used by video processing for scaling.
  Microsoft::WRL::ComPtr<ID3D11VideoProcessor> video_processor_;
  Microsoft::WRL::ComPtr<ID3D11VideoProcessorEnumerator>
      video_processor_enumerator_;
  Microsoft::WRL::ComPtr<ID3D11VideoDevice> video_device_;
  Microsoft::WRL::ComPtr<ID3D11VideoContext> video_context_;
  D3D11_VIDEO_PROCESSOR_CONTENT_DESC vp_desc_ = {};
  Microsoft::WRL::ComPtr<ID3D11Texture2D> scaled_d3d11_texture_;
  Microsoft::WRL::ComPtr<ID3D11VideoProcessorOutputView> vp_output_view_;
  // Destination texture used by the copy operation.
  Microsoft::WRL::ComPtr<ID3D11Texture2D> copied_d3d11_texture_;

  // To expose client callbacks from VideoEncodeAccelerator.
  raw_ptr<Client> client_ = nullptr;
  SEQUENCE_CHECKER(sequence_checker_);

  // DXGI device manager for handling hardware input textures
  scoped_refptr<DXGIDeviceManager> dxgi_device_manager_;
  // Mapping of dxgi resource needed when HMFT rejects setting D3D11 manager.
  bool dxgi_resource_mapping_required_ = false;
  // Staging texture for copying from GPU memory if HMFT does not operate in
  // D3D11 mode.
  Microsoft::WRL::ComPtr<ID3D11Texture2D> staging_texture_;

  // Preferred adapter for DXGIDeviceManager.
  const CHROME_LUID luid_;

  // A buffer used as a scratch space for I420 to NV12 conversion
  std::vector<uint8_t> resize_buffer_;

  FlushCallback flush_callback_;

  // Bitrate controller for CBR encoding.
  std::unique_ptr<VideoRateControlWrapper> rate_ctrl_;

  // Color space of the first frame sent to Encode(). Every input gets an entry
  // in `output_color_spaces_`, new outputs take the color space from the front.
  base::circular_deque<gfx::ColorSpace> output_color_spaces_;

  // Declared last to ensure that all weak pointers are invalidated before
  // other destructors run.
  base::WeakPtr<MediaFoundationVideoEncodeAccelerator> weak_ptr_;
  base::WeakPtrFactory<MediaFoundationVideoEncodeAccelerator> weak_factory_{
      this};
};

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_MEDIA_FOUNDATION_VIDEO_ENCODE_ACCELERATOR_WIN_H_
