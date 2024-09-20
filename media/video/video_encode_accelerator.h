// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_VIDEO_ENCODE_ACCELERATOR_H_
#define MEDIA_VIDEO_VIDEO_ENCODE_ACCELERATOR_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <optional>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "media/base/bitrate.h"
#include "media/base/encoder_status.h"
#include "media/base/media_export.h"
#include "media/base/svc_scalability_mode.h"
#include "media/base/video_bitrate_allocation.h"
#include "media/base/video_codecs.h"
#include "media/base/video_encoder.h"
#include "media/base/video_types.h"
#include "media/video/video_encoder_info.h"
#include "ui/gfx/color_space.h"

namespace media {

class BitstreamBuffer;
class MediaLog;
class VideoFrame;
class CommandBufferHelper;

// Metadata for a dropped frame.
// BitstreamBufferMetadata has this data if and only if the frame is
// dropped.
// |spatial_idx|    indicates the spatial index for this frame.
// |end_of_picture| is trueiff frame is last spatial layer frame of picture.
struct MEDIA_EXPORT DropFrameMetadata final {
  uint8_t spatial_idx = 0;
  bool end_of_picture = true;
};

//  Metadata for a H264 bitstream buffer.
//  |temporal_idx|  indicates the temporal index for this frame.
//  |layer_sync|    is true iff this frame has |temporal_idx| > 0 and does NOT
//                  reference any reference buffer containing a frame with
//                  temporal_idx > 0.
struct MEDIA_EXPORT H264Metadata final {
  uint8_t temporal_idx = 0;
  bool layer_sync = false;
};

// Metadata for H265 bitstream buffer.
//  |temporal_idx|  indicates the temporal index of this frame.
struct MEDIA_EXPORT H265Metadata final {
  uint8_t temporal_idx = 0;
};

//  Metadata for a VP8 bitstream buffer.
//  |non_reference| is true iff this frame does not update any reference buffer,
//                  meaning dropping this frame still results in a decodable
//                  stream.
//  |temporal_idx|  indicates the temporal index for this frame.
//  |layer_sync|    is true iff this frame has |temporal_idx| > 0 and does NOT
//                  reference any reference buffer containing a frame with
//                  temporal_idx > 0.
struct MEDIA_EXPORT Vp8Metadata final {
  bool non_reference = false;
  uint8_t temporal_idx = 0;
  bool layer_sync = false;
};

// Metadata for a VP9 bitstream buffer, this struct resembles
// webrtc::CodecSpecificInfoVP9 [1]
// https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/modules/video_coding/include/video_codec_interface.h;l=56;drc=e904161cecbe5e2ca31382e2a62fc776151bb8f2
struct MEDIA_EXPORT Vp9Metadata final {
  Vp9Metadata();
  ~Vp9Metadata();
  Vp9Metadata(const Vp9Metadata&);

  // True iff this layer frame is dependent on previously coded frame(s).
  bool inter_pic_predicted = false;
  // True iff this frame only references TL0 frames.
  bool temporal_up_switch = false;
  // True iff frame is referenced by upper spatial layer frame.
  bool referenced_by_upper_spatial_layers = false;
  // True iff frame is dependent on directly lower spatial layer frame.
  bool reference_lower_spatial_layers = false;
  // True iff frame is last spatial layer frame of picture.
  bool end_of_picture = true;

  // The temporal index for this frame.
  uint8_t temporal_idx = 0;
  // The spatial index for this frame. This is based on the index which always
  // start with zero. In other words, the |spatial_idx| is in the range of [0,
  // |end_active_spatial_layer_index| - |begin_active_spatial_layer_index|).
  uint8_t spatial_idx = 0;
  // The resolutions of active spatial layers, filled if and only if keyframe or
  // the number of active spatial layers is changed. This contains only the
  // resolutions of the active spatial layers. Therefore, every gfx::Size is not
  // gfx::Size(0, 0).
  std::vector<gfx::Size> spatial_layer_resolutions;
  // The active spatial layer indices in the spatial layers configured in
  // Initialize(). This is filled if and only if |spatial_layer_resolutions| is
  // filled. |spatial_layer_resolutions.size()| is always the same as
  // |end_active_spatial_layer_index| - |begin_active_spatial_layer_index|.
  // The GPU process ensures they are less than three, or
  // VP9SVCLayers::kMaxSpatialLayers. But when we send the struct over mojom, we
  // don't ensure them in the traits. So the receiver in blink also checks these
  // validity in the renderer process.
  uint8_t begin_active_spatial_layer_index = 0;
  uint8_t end_active_spatial_layer_index = 0;

  // The differences between the picture id of this frame and picture ids
  // of reference frames, only be filled for non key frames.
  std::vector<uint8_t> p_diffs;
};

// Metadata for an AV1 bitstream buffer.
struct MEDIA_EXPORT Av1Metadata final {
  Av1Metadata();
  ~Av1Metadata();
  Av1Metadata(const Av1Metadata&);
  // The temporal index for this frame.
  uint8_t temporal_idx = 0;
};

//  Metadata associated with a bitstream buffer.
//  |payload_size| is the byte size of the used portion of the buffer.
//  |key_frame| is true if this delivered frame is a keyframe.
//  |timestamp| is the same timestamp as in VideoFrame passed to Encode().
//  |qp| is the quantizer value, default invalid qp value is -1 following
//  webrtc::EncodedImage.
struct MEDIA_EXPORT BitstreamBufferMetadata final {
  BitstreamBufferMetadata();
  BitstreamBufferMetadata(const BitstreamBufferMetadata& other);
  BitstreamBufferMetadata& operator=(const BitstreamBufferMetadata& other);
  BitstreamBufferMetadata(BitstreamBufferMetadata&& other);
  BitstreamBufferMetadata(size_t payload_size_bytes,
                          bool key_frame,
                          base::TimeDelta timestamp);
  static BitstreamBufferMetadata CreateForDropFrame(base::TimeDelta timestamp,
                                                    uint8_t spatial_idx = 0,
                                                    bool end_of_picture = true);
  ~BitstreamBufferMetadata();

  // If |payload_size_bytes| is zero, it indicates the frame corresponded to
  // |timestamp| is dropped.
  size_t payload_size_bytes;
  bool key_frame;
  base::TimeDelta timestamp;
  int32_t qp = -1;

  // This is true if a frame is the last spatial layer frame in SVC encoding.
  // This is always true in non-SVC encoding.
  bool end_of_picture() const;
  bool dropped_frame() const;
  std::optional<uint8_t> spatial_idx() const;

  // |drop|, |h264|, |vp8|, |vp9|, |av1| and |h265| may be set, but not multiple
  // of them. Presumably, it's also possible for none of them to be set.
  // |drop| is set if and only if the frame is dropped.
  std::optional<DropFrameMetadata> drop;
  std::optional<H264Metadata> h264;
  std::optional<Vp8Metadata> vp8;
  std::optional<Vp9Metadata> vp9;
  std::optional<Av1Metadata> av1;
  std::optional<H265Metadata> h265;

  // Some platforms may adjust the encoding size to meet hardware requirements.
  // If not set, the encoded size is the same as configured.
  std::optional<gfx::Size> encoded_size;

  // Some platforms may adjust the color space.
  std::optional<gfx::ColorSpace> encoded_color_space;
};

// Video encoder interface.
class MEDIA_EXPORT VideoEncodeAccelerator {
 public:
  // Bitmask values for supported rate control modes.
  enum SupportedRateControlMode : uint8_t {
    kNoMode = 0,  // for uninitialized profiles only
    kConstantMode = 0b0001,
    kVariableMode = 0b0010,
    kExternalMode = 0b0100,
  };

  // Specification of an encoding profile supported by an encoder.
  struct MEDIA_EXPORT SupportedProfile {
    SupportedProfile();
    SupportedProfile(
        VideoCodecProfile profile,
        const gfx::Size& max_resolution,
        uint32_t max_framerate_numerator = 0u,
        uint32_t max_framerate_denominator = 1u,
        SupportedRateControlMode rc_modes = kConstantMode,
        const std::vector<SVCScalabilityMode>& scalability_modes = {},
        const std::vector<VideoPixelFormat>& gpu_suppoted_pixel_formats = {},
        bool supports_shared_images = false);
    SupportedProfile(const SupportedProfile& other);
    SupportedProfile& operator=(const SupportedProfile& other) = default;
    ~SupportedProfile();
    VideoCodecProfile profile;
    gfx::Size min_resolution;
    gfx::Size max_resolution;
    uint32_t max_framerate_numerator{0};
    uint32_t max_framerate_denominator{0};
    SupportedRateControlMode rate_control_modes = kNoMode;
    std::vector<SVCScalabilityMode> scalability_modes;
    bool is_software_codec = false;
    std::vector<VideoPixelFormat> gpu_supported_pixel_formats;
    bool supports_gpu_shared_images = false;
  };
  using SupportedProfiles = std::vector<SupportedProfile>;
  using FlushCallback = base::OnceCallback<void(bool)>;

  // A default framerate for all VEA implementations.
  enum { kDefaultFramerate = 30 };

  // Parameters required for VEA initialization.
  struct MEDIA_EXPORT Config {
    // Indicates if video content should be treated as a "normal" camera feed
    // or as generated (e.g. screen capture).
    enum class ContentType { kCamera, kDisplay };

    // Indicates the storage type of a video frame provided on Encode().
    // kShmem if a video frame has a shared memory.
    // kGpuMemoryBuffer if a video frame has a GpuMemoryBuffer.
    enum class StorageType { kShmem, kGpuMemoryBuffer };

    // Used to require a certain encoder type is selected. The default is that
    // hardware is required.
    enum class EncoderType { kHardware, kSoftware, kNoPreference };

    struct MEDIA_EXPORT SpatialLayer {
      // The encoder dimension of the spatial layer.
      int32_t width = 0;
      int32_t height = 0;
      // The bitrate of encoded output stream of the spatial layer in bits per
      // second.
      uint32_t bitrate_bps = 0u;
      uint32_t framerate = 0u;
      // The recommended maximum qp value of the spatial layer. VEA can ignore
      // this value.
      uint8_t max_qp = 0u;
      // The number of temporal layers of the spatial layer. The detail of
      // the temporal layer structure is up to VideoEncodeAccelerator.
      uint8_t num_of_temporal_layers = 0u;
    };

    Config();
    Config(const Config& config);

    Config(VideoPixelFormat input_format,
           const gfx::Size& input_visible_size,
           VideoCodecProfile output_profile,
           const Bitrate& bitrate,
           uint32_t framerate,
           StorageType storage_type,
           ContentType content_type);

    ~Config();

    std::string AsHumanReadableString() const;

    bool HasTemporalLayer() const;
    bool HasSpatialLayer() const;

    // Frame format of input stream (as would be reported by
    // VideoFrame::format() for frames passed to Encode()).
    VideoPixelFormat input_format;

    // Resolution of input stream (as would be reported by
    // VideoFrame::visible_rect().size() for frames passed to Encode()).
    gfx::Size input_visible_size;

    // Codec profile of encoded output stream.
    VideoCodecProfile output_profile;

    // Configuration details for the bitrate, indicating the bitrate mode (ex.
    // variable or constant) and target bitrate.
    Bitrate bitrate;

    // The encoder frame rate in frames per second.
    uint32_t framerate;

    // The storage type of video frame provided on Encode().
    // This is kShmem iff a video frame is mapped in user space.
    // This is kDmabuf iff a video frame has dmabuf.
    StorageType storage_type;

    // Indicates captured video (from a camera) or generated (screen grabber).
    // Screen content has a number of special properties such as lack of noise,
    // burstiness of motion and requirements for readability of small text in
    // bright colors. With this content hint the encoder may choose to optimize
    // for the given use case.
    ContentType content_type;

    // Group of picture length for encoded output stream, indicates the
    // distance between two key frames, i.e. IPPPIPPP would be represent as 4.
    std::optional<uint32_t> gop_length;

    // Codec level of encoded output stream for H264 only. This value should
    // be aligned to the H264 standard definition of SPS.level_idc.
    // If this is not given, VideoEncodeAccelerator selects one of proper H.264
    // levels for |input_visible_size| and |framerate|.
    std::optional<uint8_t> h264_output_level;

    // Indicates baseline profile or constrained baseline profile for H264 only.
    bool is_constrained_h264 = false;

    // |drop_frame_thresh_percentage| is described as a percentage of the target
    // data buffer. When the data buffer falls below this percentage of
    // fullness, a dropped frame is indicated. The default value is zero, which
    // means a VideoEncodeAccelerator doesn't allow to drop a frame.
    // Two caveats:
    // (1) VideoToolboxVideoEncodeAccelerator (macOS) doesn't provide a way to
    // disallow drop a frame. That's said, the VideoEncodeAccelerator may drop
    // a frame even if |drop_frame_thresh_percentage| is set to zero.
    // (2) A VideoENcodeAccelerator doesn't necessarily support a frame drop.
    // Therefore a frame may not be dropped even if
    // |drop_frame_thresh_percentage| is set to a positive value.
    uint8_t drop_frame_thresh_percentage = 0;

    // The configuration for spatial layers. This is not empty if and only if
    // either spatial or temporal layer encoding is configured. When this is not
    // empty, VideoEncodeAccelerator should refer the width, height, bitrate and
    // etc. of |spatial_layers|.
    std::vector<SpatialLayer> spatial_layers;

    // Indicates the inter layer prediction mode for SVC encoding.
    SVCInterLayerPredMode inter_layer_pred = SVCInterLayerPredMode::kOnKeyPic;

    // This flag forces the encoder to use low latency mode, suitable for
    // RTC use cases.
    bool require_low_delay = true;

    // Indicates what type of encoder is required. Useful when OS software
    // encoders may be present and/or superior to built-in encoders.
    EncoderType required_encoder_type = EncoderType::kHardware;
  };

  // Interface for clients that use VideoEncodeAccelerator. These callbacks will
  // not be made unless Initialize() has returned successfully.
  class MEDIA_EXPORT Client {
   public:
    // Callback to tell the client what size of frames and buffers to provide
    // for input and output.  The VEA disclaims use or ownership of all
    // previously provided buffers once this callback is made.
    // Parameters:
    //  |input_count| is the number of input VideoFrames required for encoding.
    //  The client should be prepared to feed at least this many frames into the
    //  encoder before being returned any input frames, since the encoder may
    //  need to hold onto some subset of inputs as reference pictures.
    //  |input_coded_size| is the logical size of the input frames (as reported
    //  by VideoFrame::coded_size()) to encode, in pixels.  The encoder may have
    //  hardware alignment requirements that make this different from
    //  |input_visible_size|, as requested in Initialize(), in which case the
    //  input VideoFrame to Encode() should be padded appropriately.
    //  |output_buffer_size| is the required size of output buffers for this
    //  encoder in bytes.
    virtual void RequireBitstreamBuffers(unsigned int input_count,
                                         const gfx::Size& input_coded_size,
                                         size_t output_buffer_size) = 0;

    // Callback to deliver encoded bitstream buffers.  Ownership of the buffer
    // is transferred back to the VEA::Client once this callback is made.
    // Parameters:
    //  |bitstream_buffer_id| is the id of the buffer that is ready.
    //  |metadata| contains data such as payload size and timestamp. See above.
    virtual void BitstreamBufferReady(
        int32_t bitstream_buffer_id,
        const BitstreamBufferMetadata& metadata) = 0;

    // Error notification callback. Note that errors in Initialize() will not be
    // reported here, but will instead be indicated by a false return value
    // there.
    virtual void NotifyErrorStatus(const EncoderStatus& status) = 0;

    // Call VideoEncoderInfo of the VEA is changed.
    virtual void NotifyEncoderInfoChange(const VideoEncoderInfo& info);

   protected:
    // Clients are not owned by VEA instances and should not be deleted through
    // these pointers.
    virtual ~Client() {}
  };

  // Video encoder functions.

  // Returns a list of the supported codec profiles of the video encoder. This
  // can be called before Initialize().
  virtual SupportedProfiles GetSupportedProfiles() = 0;

  // Initializes the video encoder with specific configuration.  Called once per
  // encoder construction.  This call is synchronous and returns true iff
  // initialization is successful.
  // TODO(mcasas): Update to asynchronous, https://crbug.com/744210.
  // Parameters:
  //  |config| contains the initialization parameters.
  //  |client| is the client of this video encoder.  The provided pointer must
  //  be valid until Destroy() is called.
  //  |media_log| is used to report error messages.
  // TODO(sheu): handle resolution changes.  http://crbug.com/249944
  virtual bool Initialize(const Config& config,
                          Client* client,
                          std::unique_ptr<MediaLog> media_log) = 0;

  // Encodes the given frame.
  // The storage type of |frame| must be the |storage_type| if it is specified
  // in Initialize().
  // Parameters:
  //  |frame| is the VideoFrame that is to be encoded.
  //  |force_keyframe| forces the encoding of a keyframe for this frame.
  virtual void Encode(scoped_refptr<VideoFrame> frame, bool force_keyframe) = 0;

  // Encodes the given frame.
  // Parameters:
  //  |frame| is the VideoFrame that is to be encoded.
  //  |options| provides extra details for encoding |frame|.
  virtual void Encode(scoped_refptr<VideoFrame> frame,
                      const VideoEncoder::EncodeOptions& options);

  // Send a bitstream buffer to the encoder to be used for storing future
  // encoded output.  Each call here with a given |buffer| will cause the buffer
  // to be filled once, then returned with BitstreamBufferReady().
  // Parameters:
  //  |buffer| is the bitstream buffer to use for output.
  virtual void UseOutputBitstreamBuffer(BitstreamBuffer buffer) = 0;

  // Request a change to the encoding parameters.
  // Parameters:
  //  |bitrate| is the requested new bitrate. The bitrate mode cannot be changed
  //  using this method and attempting to do so will result in an error.
  //  Instead, re-create a VideoEncodeAccelerator.
  //  |framerate| is the requested new framerate, in frames per second.
  //  |size| is the requested new input visible frame size. Clients can request
  //  frame size change only when there is no pending frame in the encoder.
  // Note:
  // Implementation must call |RequireBitstreamBuffers| when frame size changes,
  // even existing buffer can be reused. This is a workaround for
  // |VideoEncodeAcceleratorAdapter| to know when reconfigure is done. This
  // requirement can be removed once |RequestEncodingParametersChange| has a
  // callback.
  virtual void RequestEncodingParametersChange(
      const Bitrate& bitrate,
      uint32_t framerate,
      const std::optional<gfx::Size>& size) = 0;

  // Request a change to the encoding parameters. If not implemented, default
  // behavior is to get the sum over layers and pass to version with bitrate
  // as uint32_t.
  // Parameters:
  //  |bitrate| is the requested new bitrate, per spatial and temporal layer.
  //  |framerate| is the requested new framerate, in frames per second.
  //  |size| is the requested new input visible frame size. Clients can request
  //  frame size change only when there is no pending frame in the encoder.
  // Note:
  // Implementation must call |RequireBitstreamBuffers| when frame size changes,
  // even existing buffer can be reused. This is a workaround for
  // |VideoEncodeAcceleratorAdapter| to know when reconfigure is done. This
  // requirement can be removed once |RequestEncodingParametersChange| has a
  // callback.
  virtual void RequestEncodingParametersChange(
      const VideoBitrateAllocation& bitrate,
      uint32_t framerate,
      const std::optional<gfx::Size>& size);

  // Destroys the encoder: all pending inputs and outputs are dropped
  // immediately and the component is freed.  This call may asynchronously free
  // system resources, but its client-visible effects are synchronous. After
  // this method returns no more callbacks will be made on the client. Deletes
  // |this| unconditionally, so make sure to drop all pointers to it!
  virtual void Destroy() = 0;

  // Flushes the encoder: all pending inputs will be encoded and all bitstreams
  // handed back to the client, and afterwards the |flush_callback| will be
  // called. The FlushCallback takes a boolean argument: |true| indicates the
  // flush is complete; |false| indicates the flush is cancelled due to errors
  // or destruction. The client should not invoke Flush() or Encode() while the
  // previous Flush() is not finished yet.
  virtual void Flush(FlushCallback flush_callback);

  // Returns true if the encoder support flush. This method must be called after
  // VEA has been initialized.
  virtual bool IsFlushSupported();

  // Returns true if the encoder supports automatic resize of GPU backed frames
  // to the size provided during encoder configuration.
  // This method must be called after VEA has been initialized.
  virtual bool IsGpuFrameResizeSupported();

  // Provides a callback to acquire a CommandBufferStub, which may be used
  // to access shared images.
  virtual void SetCommandBufferHelperCB(
      base::RepeatingCallback<scoped_refptr<CommandBufferHelper>()>
          get_command_buffer_helper_cb,
      scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner);

 protected:
  // Do not delete directly; use Destroy() or own it with a unique_ptr, which
  // will Destroy() it properly by default.
  virtual ~VideoEncodeAccelerator();
};

MEDIA_EXPORT bool operator==(const VideoEncodeAccelerator::SupportedProfile& l,
                             const VideoEncodeAccelerator::SupportedProfile& r);
MEDIA_EXPORT bool operator==(const Vp8Metadata& l, const Vp8Metadata& r);
MEDIA_EXPORT bool operator==(const Vp9Metadata& l, const Vp9Metadata& r);
MEDIA_EXPORT bool operator==(const Av1Metadata& l, const Av1Metadata& r);
MEDIA_EXPORT bool operator==(const BitstreamBufferMetadata& l,
                             const BitstreamBufferMetadata& r);
MEDIA_EXPORT bool operator==(
    const VideoEncodeAccelerator::Config::SpatialLayer& l,
    const VideoEncodeAccelerator::Config::SpatialLayer& r);
MEDIA_EXPORT bool operator==(const VideoEncodeAccelerator::Config& l,
                             const VideoEncodeAccelerator::Config& r);

MEDIA_EXPORT inline VideoEncodeAccelerator::SupportedRateControlMode operator|(
    VideoEncodeAccelerator::SupportedRateControlMode lhs,
    VideoEncodeAccelerator::SupportedRateControlMode rhs) {
  return static_cast<VideoEncodeAccelerator::SupportedRateControlMode>(
      static_cast<uint8_t>(lhs) | static_cast<uint8_t>(rhs));
}

MEDIA_EXPORT inline VideoEncodeAccelerator::SupportedRateControlMode&
operator|=(VideoEncodeAccelerator::SupportedRateControlMode& lhs,
           VideoEncodeAccelerator::SupportedRateControlMode rhs) {
  lhs = lhs | rhs;
  return lhs;
}

MEDIA_EXPORT inline VideoEncodeAccelerator::SupportedRateControlMode operator&(
    VideoEncodeAccelerator::SupportedRateControlMode lhs,
    VideoEncodeAccelerator::SupportedRateControlMode rhs) {
  return static_cast<VideoEncodeAccelerator::SupportedRateControlMode>(
      static_cast<uint8_t>(lhs) & static_cast<uint8_t>(rhs));
}

MEDIA_EXPORT inline VideoEncodeAccelerator::SupportedRateControlMode&
operator&=(VideoEncodeAccelerator::SupportedRateControlMode& lhs,
           VideoEncodeAccelerator::SupportedRateControlMode rhs) {
  lhs = lhs & rhs;
  return lhs;
}
}  // namespace media

namespace std {

// Specialize std::default_delete so that
// std::unique_ptr<VideoEncodeAccelerator> uses "Destroy()" instead of trying to
// use the destructor.
template <>
struct MEDIA_EXPORT default_delete<media::VideoEncodeAccelerator> {
  void operator()(media::VideoEncodeAccelerator* vea) const;
};

}  // namespace std

#endif  // MEDIA_VIDEO_VIDEO_ENCODE_ACCELERATOR_H_
