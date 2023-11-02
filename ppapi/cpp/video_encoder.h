// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_VIDEO_ENCODER_H_
#define PPAPI_CPP_VIDEO_ENCODER_H_

#include <stdint.h>

#include "ppapi/c/pp_codecs.h"
#include "ppapi/c/pp_size.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/resource.h"
#include "ppapi/cpp/size.h"
#include "ppapi/cpp/video_frame.h"

/// @file
/// This file defines the API to create and use a VideoEncoder resource.

namespace pp {

class InstanceHandle;

/// Video encoder interface.
///
/// Typical usage:
/// - Call Create() to create a new video encoder resource.
/// - Call GetSupportedFormats() to determine which codecs and profiles are
///   available.
/// - Call Initialize() to initialize the encoder for a supported profile.
/// - Call GetVideoFrame() to get a blank frame and fill it in, or get a video
///   frame from another resource, e.g. <code>PPB_MediaStreamVideoTrack</code>.
/// - Call Encode() to push the video frame to the encoder. If an external frame
///   is pushed, wait for completion to recycle the frame.
/// - Call GetBitstreamBuffer() continuously (waiting for each previous call to
///   complete) to pull encoded pictures from the encoder.
/// - Call RecycleBitstreamBuffer() after consuming the data in the bitstream
///   buffer.
/// - To destroy the encoder, the plugin should release all of its references to
///   it. Any pending callbacks will abort before the encoder is destroyed.
///
/// Available video codecs vary by platform.
/// All: vp8 (software).
/// ChromeOS, depending on your device: h264 (hardware), vp8 (hardware)
class VideoEncoder : public Resource {
 public:
  /// Default constructor for creating an is_null() <code>VideoEncoder</code>
  /// object.
  VideoEncoder();

  /// A constructor used to create a <code>VideoEncoder</code> and associate it
  /// with the provided <code>Instance</code>.
  /// @param[in] instance The instance with which this resource will be
  /// associated.
  explicit VideoEncoder(const InstanceHandle& instance);

  /// The copy constructor for <code>VideoEncoder</code>.
  /// @param[in] other A reference to a <code>VideoEncoder</code>.
  VideoEncoder(const VideoEncoder& other);
  VideoEncoder& operator=(const VideoEncoder& other);

  /// Gets an array of supported video encoder profiles.
  /// These can be used to choose a profile before calling Initialize().
  ///
  /// @param[in] callback A <code>CompletionCallbackWithOutput</code> to be
  /// called upon completion with the PP_VideoProfileDescription structs.
  ///
  /// @return If >= 0, the number of supported profiles returned, otherwise an
  /// error code from <code>pp_errors.h</code>.
  int32_t GetSupportedProfiles(const CompletionCallbackWithOutput<
      std::vector<PP_VideoProfileDescription> >& cc);

  /// Initializes a video encoder resource. This should be called after
  /// GetSupportedProfiles() and before any functions below.
  ///
  /// @param[in] input_format The <code>PP_VideoFrame_Format</code> of the
  /// frames which will be encoded.
  /// @param[in] input_visible_size A <code>Size</code> specifying the
  /// dimensions of the visible part of the input frames.
  /// @param[in] output_profile A <code>PP_VideoProfile</code> specifying the
  /// codec profile of the encoded output stream.
  /// @param[in] acceleration A <code>PP_HardwareAcceleration</code> specifying
  /// whether to use a hardware accelerated or a software implementation.
  /// @param[in] callback A <code>CompletionCallback</code> to be called upon
  /// completion.
  ///
  /// @return An int32_t containing an error code from <code>pp_errors.h</code>.
  /// Returns PP_ERROR_NOTSUPPORTED if video encoding is not available, or the
  /// requested codec profile is not supported.
  /// Returns PP_ERROR_NOMEMORY if frame and bitstream buffers can't be created.
  int32_t Initialize(const PP_VideoFrame_Format& input_format,
                     const Size& input_visible_size,
                     const PP_VideoProfile& output_profile,
                     const uint32_t initial_bitrate,
                     PP_HardwareAcceleration acceleration,
                     const CompletionCallback& cc);

  /// Gets the number of input video frames that the encoder may hold while
  /// encoding. If the plugin is providing the video frames, it should have at
  /// least this many available.
  ///
  /// @return An int32_t containing the number of frames required, or an error
  /// code from <code>pp_errors.h</code>.
  /// Returns PP_ERROR_FAILED if Initialize() has not successfully completed.
  int32_t GetFramesRequired();

  /// Gets the coded size of the video frames required by the encoder. Coded
  /// size is the logical size of the input frames, in pixels.  The encoder may
  /// have hardware alignment requirements that make this different from
  /// |input_visible_size|, as requested in the call to Initialize().
  ///
  /// @param[in] coded_size A <code>Size</code> to hold the coded size.
  ///
  /// @return An int32_t containing a result code from <code>pp_errors.h</code>.
  /// Returns PP_ERROR_FAILED if Initialize() has not successfully completed.
  int32_t GetFrameCodedSize(Size* coded_size);

  /// Gets a blank video frame which can be filled with video data and passed
  /// to the encoder.
  ///
  /// @param[in] callback A <code>CompletionCallbackWithOutput</code> to be
  /// called upon completion with the blank <code>VideoFrame</code> resource.
  ///
  /// @return An int32_t containing an error code from <code>pp_errors.h</code>.
  int32_t GetVideoFrame(const CompletionCallbackWithOutput<VideoFrame>& cc);

  /// Encodes a video frame.
  ///
  /// @param[in] video_frame The <code>VideoFrame</code> to be encoded.
  /// @param[in] force_keyframe A <code>PP_Bool> specifying whether the encoder
  /// should emit a key frame for this video frame.
  /// @param[in] callback A <code>CompletionCallback</code> to be called upon
  /// completion. Plugins that pass <code>VideoFrame</code> resources owned
  /// by other resources should wait for completion before reusing them.
  ///
  /// @return An int32_t containing an error code from <code>pp_errors.h</code>.
  /// Returns PP_ERROR_FAILED if Initialize() has not successfully completed.
  int32_t Encode(const VideoFrame& video_frame,
                 bool force_keyframe,
                 const CompletionCallback& cc);

  /// Gets the next encoded bitstream buffer from the encoder.
  ///
  /// @param[out] bitstream_buffer A <code>PP_BitstreamBuffer</code> containing
  /// encoded video data.
  /// @param[in] callback A <code>CompletionCallbackWithOutput</code> to be
  /// called upon completion with the next bitstream buffer. The plugin can call
  /// GetBitstreamBuffer from the callback in order to continuously "pull"
  /// bitstream buffers from the encoder.
  ///
  /// @return An int32_t containing an error code from <code>pp_errors.h</code>.
  /// Returns PP_ERROR_FAILED if Initialize() has not successfully completed.
  /// Returns PP_ERROR_INPROGRESS if a prior call to GetBitstreamBuffer() has
  /// not completed.
  int32_t GetBitstreamBuffer(
      const CompletionCallbackWithOutput<PP_BitstreamBuffer>& cc);

  /// Recycles a bitstream buffer back to the encoder.
  ///
  /// @param[in] bitstream_buffer A <code>PP_BitstreamBuffer</code> that is no
  /// longer needed by the plugin.
  void RecycleBitstreamBuffer(const PP_BitstreamBuffer& bitstream_buffer);

  /// Requests a change to encoding parameters. This is only a request,
  /// fulfilled on a best-effort basis.
  ///
  /// @param[in] bitrate The requested new bitrate, in bits per second.
  /// @param[in] framerate The requested new framerate, in frames per second.
  void RequestEncodingParametersChange(uint32_t bitrate, uint32_t framerate);

  /// Closes the video encoder, and cancels any pending encodes. Any pending
  /// callbacks will still run, reporting <code>PP_ERROR_ABORTED</code> . It is
  /// not valid to call any encoder functions after a call to this method.
  /// <strong>Note:</strong> Destroying the video encoder closes it implicitly,
  /// so you are not required to call Close().
  void Close();
};

}  // namespace pp

#endif  // PPAPI_CPP_VIDEO_ENCODER_H_
