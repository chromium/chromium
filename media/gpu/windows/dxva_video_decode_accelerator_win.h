// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_DXVA_VIDEO_DECODE_ACCELERATOR_WIN_H_
#define MEDIA_GPU_WINDOWS_DXVA_VIDEO_DECODE_ACCELERATOR_WIN_H_

#include <d3d11_1.h>
#include <d3d9.h>
#include <initguid.h>
#include <stdint.h>
#include <wrl/client.h>

// Work around bug in this header by disabling the relevant warning for it.
// https://connect.microsoft.com/VisualStudio/feedback/details/911260/dxva2api-h-in-win8-sdk-triggers-c4201-with-w4
#pragma warning(push)
#pragma warning(disable : 4201)
#include <dxva2api.h>
#pragma warning(pop)
#include <mfidl.h>

#include <list>
#include <map>
#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "gpu/config/gpu_preferences.h"
#include "media/base/video_color_space.h"
#include "media/gpu/gpu_video_decode_accelerator_helpers.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/windows/d3d11_com_defs.h"
#include "media/video/video_decode_accelerator.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/rect.h"

interface IMFSample;
interface IDirect3DSurface9;

namespace gl {
class GLContext;
}

namespace gpu {
class GpuDriverBugWorkarounds;
struct GpuPreferences;
}  // namespace gpu

typedef HRESULT(WINAPI* CreateDXGIDeviceManager)(
    UINT* reset_token,
    IMFDXGIDeviceManager** device_manager);

namespace media {
class DXVAPictureBuffer;
class EGLStreamCopyPictureBuffer;
class EGLStreamPictureBuffer;
class MediaLog;
class PbufferPictureBuffer;

class ConfigChangeDetector {
 public:
  virtual ~ConfigChangeDetector();
  virtual bool DetectConfig(const uint8_t* stream, unsigned int size) = 0;
  virtual gfx::Rect current_visible_rect(
      const gfx::Rect& container_visible_rect) const = 0;
  virtual VideoColorSpace current_color_space(
      const VideoColorSpace& container_color_space) const = 0;
  bool config_changed() const { return config_changed_; }

 protected:
  // Set to true if we detect a stream configuration change.
  bool config_changed_ = false;
};

// Class to provide a DXVA 2.0 based accelerator using the Microsoft Media
// foundation APIs via the VideoDecodeAccelerator interface.
// This class lives on a single thread and DCHECKs that it is never accessed
// from any other.
class MEDIA_GPU_EXPORT DXVAVideoDecodeAccelerator
    : public VideoDecodeAccelerator {
 public:
  enum State {
    kUninitialized,  // un-initialized.
    kNormal,         // normal playing state.
    kResetting,      // upon received Reset(), before ResetDone()
    kStopped,        // upon output EOS received.
    kFlushing,       // upon flush request received.
    kConfigChange,   // stream configuration change detected.
  };

  // Does not take ownership of |client| which must outlive |*this|.
  DXVAVideoDecodeAccelerator(
      const GetGLContextCallback& get_gl_context_cb,
      const MakeGLContextCurrentCallback& make_context_current_cb,
      const BindGLImageCallback& bind_image_cb,
      const gpu::GpuDriverBugWorkarounds& workarounds,
      const gpu::GpuPreferences& gpu_preferences,
      MediaLog* media_log);
  ~DXVAVideoDecodeAccelerator() override;

  // VideoDecodeAccelerator implementation.
  bool Initialize(const Config& config, Client* client) override;
  void Decode(BitstreamBuffer bitstream) override;
  void Decode(scoped_refptr<DecoderBuffer> buffer,
              int32_t bitstream_id) override;
  void AssignPictureBuffers(const std::vector<PictureBuffer>& buffers) override;
  void ReusePictureBuffer(int32_t picture_buffer_id) override;
  void Flush() override;
  void Reset() override;
  void Destroy() override;
  bool TryToSetupDecodeOnSeparateThread(
      const base::WeakPtr<Client>& decode_client,
      const scoped_refptr<base::SingleThreadTaskRunner>& decode_task_runner)
      override;
  GLenum GetSurfaceInternalFormat() const override;

  static VideoDecodeAccelerator::SupportedProfiles GetSupportedProfiles(
      const gpu::GpuPreferences& gpu_preferences,
      const gpu::GpuDriverBugWorkarounds& workarounds);

  // Preload dlls required for decoding.
  static void PreSandboxInitialization();

 private:
  friend class DXVAPictureBuffer;
  friend class EGLStreamDelayedCopyPictureBuffer;
  friend class EGLStreamCopyPictureBuffer;
  friend class EGLStreamPictureBuffer;
  friend class PbufferPictureBuffer;
  typedef void* EGLConfig;
  typedef void* EGLSurface;
  typedef std::list<Microsoft::WRL::ComPtr<IMFSample>> PendingInputs;

  // These are used for histograms, so don't change their numeric value (except
  // for kMaxValue as described below).
  enum class PictureBufferMechanism {
    // Copy to either a BGRA8 or FP16 texture using the video processor.
    COPY_TO_RGB = 0,

    // Copy to another NV12 texture that can be used in ANGLE.
    COPY_TO_NV12 = 1,

    // Bind the resulting GLImage to the NV12 texture. If the texture's used
    // in a an overlay than use it directly, otherwise copy it to another NV12
    // texture when necessary.
    DELAYED_COPY_TO_NV12 = 2,

    // Bind the NV12 decoder texture directly to the texture used in ANGLE.
    BIND = 3,

    // For UMA.  Must be the last entry.  It should be initialized to the
    // numerically largest value above; if you add more entries, then please
    // update this to be the last one.
    kMaxValue = BIND
  };

  enum class DXVALifetimeProgression {
    kInitializeStarted = 0,
    kInitializeSucceeded = 1,
    kPlaybackSucceeded = 2,

    // For UMA. Must be the last entry. It should be initialized to the
    // numerically largest value above; if you add more entries, then please
    // update this to the last one.
    kMaxValue = kPlaybackSucceeded
  };

  // Log UMA progression state.
  void AddLifetimeProgressionStage(DXVALifetimeProgression stage);

  // Creates and initializes an instance of the D3D device and the
  // corresponding device manager. The device manager instance is eventually
  // passed to the IMFTransform interface implemented by the decoder.
  bool CreateD3DDevManager();

  // TODO(hubbe): COMMENT
  bool CreateVideoProcessor();

  // Creates and initializes an instance of the DX11 device and the
  // corresponding device manager. The device manager instance is eventually
  // passed to the IMFTransform interface implemented by the decoder.
  bool CreateDX11DevManager();

  // Creates, initializes and sets the media codec types for the decoder.
  bool InitDecoder(VideoCodecProfile profile);

  // Validates whether the decoder supports hardware video acceleration.
  bool CheckDecoderDxvaSupport();

  // Returns information about the input and output streams. This includes
  // alignment information, decoder support flags, minimum sample size, etc.
  bool GetStreamsInfoAndBufferReqs();

  // Registers the input and output media types on the decoder. This includes
  // the expected input and output formats.
  bool SetDecoderMediaTypes();

  // Registers the input media type for the decoder.
  bool SetDecoderInputMediaType();

  // Registers the output media type for the decoder.
  bool SetDecoderOutputMediaType(const GUID& subtype);

  // Passes a command message to the decoder. This includes commands like
  // start of stream, end of stream, flush, drain the decoder, etc.
  bool SendMFTMessage(MFT_MESSAGE_TYPE msg, int32_t param);

  // The bulk of the decoding happens here. This function handles errors,
  // format changes and processes decoded output.
  void DoDecode(const gfx::Rect& visible_rect,
                const gfx::ColorSpace& color_space);

  // Invoked when we have a valid decoded output sample. Retrieves the D3D
  // surface and maintains a copy of it which is passed eventually to the
  // client when we have a picture buffer to copy the surface contents to.
  bool ProcessOutputSample(Microsoft::WRL::ComPtr<IMFSample> sample,
                           const gfx::Rect& visible_rect,
                           const gfx::ColorSpace& color_space);

  // Processes pending output samples by copying them to available picture
  // slots.
  void ProcessPendingSamples();

  // Helper function to notify the accelerator client about the error.
  void StopOnError(VideoDecodeAccelerator::Error error);

  // Transitions the decoder to the uninitialized state. The decoder will stop
  // accepting requests in this state.
  void Invalidate();

  // Stop and join on the decoder thread.
  void StopDecoderThread();

  // Notifies the client that the input buffer identifed by input_buffer_id has
  // been processed.
  void NotifyInputBufferRead(int input_buffer_id);

  // Notifies the client that the decoder was flushed.
  void NotifyFlushDone();

  // Notifies the client that the decoder was reset.
  void NotifyResetDone();

  // Requests picture buffers from the client.
  void RequestPictureBuffers(int width, int height);

  // Notifies the client about the availability of a picture.
  void NotifyPictureReady(int picture_buffer_id,
                          int input_buffer_id,
                          const gfx::Rect& visible_rect,
                          const gfx::ColorSpace& color_space,
                          bool allow_overlay);

  // Sends pending input buffer processed acks to the client if we don't have
  // output samples waiting to be processed.
  void NotifyInputBuffersDropped(const PendingInputs& input_buffers);

  // Decodes pending input buffers.
  void DecodePendingInputBuffers();

  // Helper for handling the Flush operation.
  void FlushInternal();

  // Helper for handling the Decode operation.
  void DecodeInternal(const Microsoft::WRL::ComPtr<IMFSample>& input_sample);

  // Handles mid stream resolution changes.
  void HandleResolutionChanged(int width, int height);

  using OutputBuffers = std::map<int32_t, std::unique_ptr<DXVAPictureBuffer>>;

  // Tells the client to dismiss the stale picture buffers passed in.
  void DismissStaleBuffers(bool force);

  // Called after the client indicates we can recycle a stale picture buffer.
  void DeferredDismissStaleBuffer(int32_t picture_buffer_id);

  // Sets the state of the decoder. Called from the main thread and the decoder
  // thread. The state is changed on the main thread.
  void SetState(State state);

  // Gets the state of the decoder. Can be called from the main thread and
  // the decoder thread. Thread safe.
  State GetState();

  // Starts the thread used for decoding. Returns true on success.
  bool StartDecoderThread();

  // Returns if we have output samples waiting to be processed. We only
  // allow one output sample to be present in the output queue at any given
  // time.
  bool OutputSamplesPresent();

  // Copies the source surface |src_surface| to the destination |dest_surface|.
  // The copying is done on the decoder thread.
  void CopySurface(IDirect3DSurface9* src_surface,
                   IDirect3DSurface9* dest_surface,
                   int picture_buffer_id,
                   int input_buffer_id,
                   const gfx::ColorSpace& color_space);

  // This is a notification that the source surface |src_surface| was copied to
  // the destination |dest_surface|. Received on the main thread.
  void CopySurfaceComplete(IDirect3DSurface9* src_surface,
                           IDirect3DSurface9* dest_surface,
                           int picture_buffer_id,
                           int input_buffer_id);

  void BindPictureBufferToSample(Microsoft::WRL::ComPtr<IMFSample> sample,
                                 int picture_buffer_id,
                                 int input_buffer_id);

  // Copies the source texture |src_texture| to the destination |dest_texture|.
  // The copying is done on the decoder thread.  Returns true on success.
  bool CopyTexture(ID3D11Texture2D* src_texture,
                   ID3D11Texture2D* dest_texture,
                   Microsoft::WRL::ComPtr<IDXGIKeyedMutex> dest_keyed_mutex,
                   uint64_t keyed_mutex_value,
                   int picture_buffer_id,
                   int input_buffer_id,
                   const gfx::ColorSpace& color_space);

  // Copies the |video_frame| to the destination |dest_texture|.
  void CopyTextureOnDecoderThread(
      ID3D11Texture2D* dest_texture,
      Microsoft::WRL::ComPtr<IDXGIKeyedMutex> dest_keyed_mutex,
      uint64_t keyed_mutex_value,
      Microsoft::WRL::ComPtr<IMFSample> input_sample,
      int picture_buffer_id,
      int input_buffer_id);

  // Flushes the decoder device to ensure that the decoded surface is copied
  // to the target surface. |iterations| helps to maintain an upper limit on
  // the number of times we try to complete the flush operation.
  void FlushDecoder(int iterations,
                    IDirect3DSurface9* src_surface,
                    IDirect3DSurface9* dest_surface,
                    int picture_buffer_id,
                    int input_buffer_id);

  // Polls to wait for GPU commands to be finished on the picture buffer
  // before reusing it.
  void WaitForOutputBuffer(int32_t picture_buffer_id, int count);

  // Initialize the DX11 video processor.
  // Returns true on success.
  bool InitializeID3D11VideoProcessor(int width,
                                      int height,
                                      const gfx::ColorSpace& color_space);

  // Returns the output video frame dimensions (width, height).
  // |sample| :- This is the output sample containing the video frame.
  // |width| :- The width is returned here.
  // |height| :- The height is returned here.
  // Returns true on success.
  bool GetVideoFrameDimensions(IMFSample* sample, int* width, int* height);

  // Sets the output type on the |transform| to the GUID identified by the
  // the |output_type| parameter. The GUID can be MFVideoFormat_RGB32,
  // MFVideoFormat_ARGB32, MFVideoFormat_NV12, etc.
  // Additionally if the |width| and |height| parameters are non zero, then
  // this function also sets the MF_MT_FRAME_SIZE attribute on the type.
  // Returns true on success.
  bool SetTransformOutputType(IMFTransform* transform,
                              const GUID& output_type,
                              int width,
                              int height);

  // Checks if the resolution, bitrate etc of the stream changed. We do this
  // by keeping track of the SPS/PPS frames and if they change we assume
  // that the configuration changed.
  // Returns S_OK or S_FALSE on success.
  // The |config_changed| parameter is set to true if we detect a change in the
  // stream.
  HRESULT CheckConfigChanged(IMFSample* sample, bool* config_changed);

  // Called when we detect a stream configuration change. We reinitialize the
  // decoder here.
  void ConfigChanged(const Config& config);

  // Sets |support_share_nv12_textures_| to false and updates
  // |num_picture_buffers_requested_|.
  void DisableSharedTextureSupport();

  uint32_t GetTextureTarget() const;

  PictureBufferMechanism GetPictureBufferMechanism() const;
  bool ShouldUseANGLEDevice() const;
  ID3D11Device* D3D11Device() const;

  // To expose client callbacks from VideoDecodeAccelerator.
  VideoDecodeAccelerator::Client* client_;

  Microsoft::WRL::ComPtr<IMFTransform> decoder_;

  Microsoft::WRL::ComPtr<IDirect3D9Ex> d3d9_;
  Microsoft::WRL::ComPtr<IDirect3DDevice9Ex> d3d9_device_ex_;
  Microsoft::WRL::ComPtr<IDirect3DDeviceManager9> device_manager_;
  Microsoft::WRL::ComPtr<IDirect3DQuery9> query_;

  ComD3D11Device d3d11_device_;
  ComD3D11Device angle_device_;
  Microsoft::WRL::ComPtr<IMFDXGIDeviceManager> d3d11_device_manager_;
  Microsoft::WRL::ComPtr<ID3D10Multithread> multi_threaded_;
  ComD3D11DeviceContext d3d11_device_context_;
  ComD3D11Query d3d11_query_;

  ComD3D11VideoDevice video_device_;
  ComD3D11VideoContext video_context_;
  ComD3D11VideoProcessorEnumerator enumerator_;
  ComD3D11VideoProcessor d3d11_processor_;

  int processor_width_ = 0;
  int processor_height_ = 0;
  bool already_initialized_ = false;

  Microsoft::WRL::ComPtr<IDirectXVideoProcessorService>
      video_processor_service_;
  Microsoft::WRL::ComPtr<IDirectXVideoProcessor> processor_;
  DXVA2_ProcAmpValues default_procamp_values_;

  // Ideally the reset token would be a stack variable which is used while
  // creating the device manager. However it seems that the device manager
  // holds onto the token and attempts to access it if the underlying device
  // changes.
  // TODO(ananta): This needs to be verified.
  uint32_t dev_manager_reset_token_;

  // Reset token for the DX11 device manager.
  uint32_t dx11_dev_manager_reset_token_;

  // The EGL config to use for decoded frames.
  EGLConfig egl_config_;

  // Current state of the decoder.
  volatile State state_;

  MFT_INPUT_STREAM_INFO input_stream_info_;
  MFT_OUTPUT_STREAM_INFO output_stream_info_;

  // Contains information about a decoded sample.
  struct PendingSampleInfo {
    PendingSampleInfo(int32_t buffer_id,
                      Microsoft::WRL::ComPtr<IMFSample> sample,
                      const gfx::Rect& visible_rect,
                      const gfx::ColorSpace& color_space);
    PendingSampleInfo(const PendingSampleInfo& other);
    ~PendingSampleInfo();

    int32_t input_buffer_id;

    // The target picture buffer id where the frame would be copied to.
    // Defaults to -1.
    int picture_buffer_id;

    gfx::Rect visible_rect;

    // The color space of this picture.
    gfx::ColorSpace color_space;

    Microsoft::WRL::ComPtr<IMFSample> output_sample;
  };

  typedef std::list<PendingSampleInfo> PendingOutputSamples;

  // List of decoded output samples. Protected by |decoder_lock_|.
  PendingOutputSamples pending_output_samples_;

  // This map maintains the picture buffers passed the client for decoding.
  // The key is the picture buffer id.
  OutputBuffers output_picture_buffers_;

  // After a resolution change there may be a few output buffers which have yet
  // to be displayed so they cannot be dismissed immediately. We move them from
  // |output_picture_buffers_| to this map so they may be dismissed once they
  // become available.
  OutputBuffers stale_output_picture_buffers_;

  // Set to true if we requested picture slots from the client.
  bool pictures_requested_;

  // Counter which holds the number of input packets before a successful
  // decode.
  int inputs_before_decode_;

  // Set to true when the drain message is sent to the decoder during a flush
  // operation. Used to ensure the message is only sent once after
  // |pending_input_buffers_| is drained. Protected by |decoder_lock_|.
  bool sent_drain_message_;

  // This is the array size of the D3D11 texture that's output to by the
  // decoder. It's only used for debugging.
  uint32_t output_array_size_ = 0;

  // List of input samples waiting to be processed.
  PendingInputs pending_input_buffers_;

  // Callback to get current GLContext.
  GetGLContextCallback get_gl_context_cb_;
  // Callback to set the correct gl context.
  MakeGLContextCurrentCallback make_context_current_cb_;
  BindGLImageCallback bind_image_cb_;

  // This may be null, e.g. when not using MojoVideoDecoder.
  MediaLog* const media_log_;

  // Which codec we are decoding with hardware acceleration.
  VideoCodec codec_;
  // Thread on which the decoder operations like passing input frames,
  // getting output frames are performed. One instance of this thread
  // is created per decoder instance.
  base::Thread decoder_thread_;

  // Task runner to be used for posting tasks to the decoder thread.
  scoped_refptr<base::SingleThreadTaskRunner> decoder_thread_task_runner_;

  // Task runner to be used for posting tasks to the main thread.
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;

  // Used to synchronize access between the decoder thread and the main thread.
  base::Lock decoder_lock_;

  // Disallow rebinding WeakReference ownership to a different thread by
  // keeping a persistent reference. This avoids problems with the
  // thread safety of reaching into this class from multiple threads to
  // attain a WeakPtr.
  base::WeakPtr<DXVAVideoDecodeAccelerator> weak_ptr_;

  // Set to true if we are in the context of a Flush operation. Used to prevent
  // multiple flush done notifications being sent out.
  bool pending_flush_;

  // Use CODECAPI_AVLowLatencyMode.
  bool enable_low_latency_;

  // Supports sharing the decoded NV12 textures with ANGLE
  bool support_share_nv12_textures_;

  // Number of requested picture buffers from the client which are used to hold
  // the decoded samples.
  int num_picture_buffers_requested_;

  // Supports copying the NV12 texture to another NV12 texture to use in
  // ANGLE.
  bool support_copy_nv12_textures_;

  // Supports copying NV12 textures on the main thread to use in ANGLE.
  bool support_delayed_copy_nv12_textures_;

  // Copy video to FP16 scRGB textures.
  bool use_fp16_ = false;

  // When converting YUV to RGB, make sure we tell the blitter about the input
  // color space so that it can convert it correctly.
  bool use_color_info_ = true;

  // Defaults to false. Indicates if we should use D3D or DX11 interfaces for
  // H/W decoding.
  bool use_dx11_;

  // True when using Microsoft's VP9 HMFT for decoding.
  bool using_ms_vp9_mft_ = false;

  // True if we should use DXGI keyed mutexes to synchronize between the two
  // contexts.
  bool use_keyed_mutex_;

  // Outputs from the dx11 format converter will be in this color space.
  gfx::ColorSpace dx11_converter_output_color_space_;

  // Set to true if we are sharing ANGLE's device.
  bool using_angle_device_;
  bool using_debug_device_;

  // Enables hardware acceleration for VP9 video decoding.
  const bool enable_accelerated_vpx_decode_;

  // The media foundation H.264 decoder has problems handling changes like
  // resolution change, bitrate change etc. If we reinitialize the decoder
  // when these changes occur then, the decoder works fine. The
  // H264ConfigChangeDetector class provides functionality to check if the
  // stream configuration changed.
  std::unique_ptr<ConfigChangeDetector> config_change_detector_;

  // Contains the initialization parameters for the video.
  Config config_;

  // Set to true if we are processing a video configuration change.
  bool processing_config_changed_;

  // Contain the visible rect and color space of frames that are currently being
  // fed into the decoder. These may change at a config change.
  gfx::Rect current_visible_rect_;
  VideoColorSpace current_color_space_;

  // WeakPtrFactory for posting tasks back to |this|.
  base::WeakPtrFactory<DXVAVideoDecodeAccelerator> weak_this_factory_{this};

  // Function pointer for the MFCreateDXGIDeviceManager API.
  static CreateDXGIDeviceManager create_dxgi_device_manager_;

  DISALLOW_COPY_AND_ASSIGN(DXVAVideoDecodeAccelerator);
};

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_DXVA_VIDEO_DECODE_ACCELERATOR_WIN_H_
