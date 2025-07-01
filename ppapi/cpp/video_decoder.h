// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_VIDEO_DECODER_H_
#define PPAPI_CPP_VIDEO_DECODER_H_

#include <stdint.h>

#include "ppapi/c/pp_codecs.h"
#include "ppapi/c/pp_size.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/graphics_3d.h"
#include "ppapi/cpp/resource.h"
#include "ppapi/cpp/size.h"

/// @file
/// This file defines the API to create and use a VideoDecoder resource.

namespace pp {

class InstanceHandle;

/// Video decoder interface.
///
/// Typical usage:
/// - Call Create() to create a new video decoder resource.
/// - Call Initialize() to initialize it with a 3d graphics context and the
///   desired codec profile.
/// - Call Decode() continuously (waiting for each previous call to complete) to
///   push bitstream buffers to the decoder.
/// - Call GetPicture() continuously (waiting for each previous call to
///   complete) to pull decoded pictures from the decoder.
/// - Call Flush() to signal end of stream to the decoder and perform shutdown
///   when it completes.
/// - Call Reset() to quickly stop the decoder (e.g. to implement Seek) and wait
///   for the callback before restarting decoding at another point.
/// - To destroy the decoder, the plugin should release all of its references to
///   it. Any pending callbacks will abort before the decoder is destroyed.
///
/// Available video codecs vary by platform.
/// All: theora, vorbis, vp8.
/// Chrome and ChromeOS: aac, h264.
/// ChromeOS: mpeg4.
class VideoDecoder : public Resource {
 public:
  /// Default constructor for creating an is_null() <code>VideoDecoder</code>
  /// object.
  VideoDecoder();

  /// A constructor used to create a <code>VideoDecoder</code> and associate it
  /// with the provided <code>Instance</code>.
  /// @param[in] instance The instance with which this resource will be
  /// associated.
  explicit VideoDecoder(const InstanceHandle& instance);

  /// The copy constructor for <code>VideoDecoder</code>.
  /// @param[in] other A reference to a <code>VideoDecoder</code>.
  VideoDecoder(const VideoDecoder& other);

  /// Initializes a video decoder resource. This should be called after Create()
  /// and before any other functions.
  ///
  /// @param[in] graphics3d_context A <code>PPB_Graphics3D</code> resource to
  /// use during decoding.
  /// @param[in] profile A <code>PP_VideoProfile</code> specifying the video
  /// codec profile.
  /// @param[in] acceleration A <code>PP_HardwareAcceleration</code> specifying
  /// whether to use a hardware accelerated or a software implementation.
  /// @param[in] min_picture_count A count of pictures the plugin would like to
  /// have in flight. This is effectively the number of times the plugin can
  /// call GetPicture() and get a decoded frame without calling
  /// RecyclePicture(). The decoder has its own internal minimum count, and will
  /// take the larger of its internal and this value. A client that doesn't care
  /// can therefore just pass in zero for this argument.
  /// @param[in] callback A <code>PP_CompletionCallback</code> to be called upon
  /// completion.
  ///
  /// @return An int32_t containing an error code from <code>pp_errors.h</code>.
  /// Returns PP_ERROR_NOTSUPPORTED if video decoding is not available, or the
  /// requested profile is not supported. In this case, the client may call
  /// Initialize() again with different parameters to find a good configuration.
  int32_t Initialize(const Graphics3D& graphics3d_context,
                     PP_VideoProfile profile,
                     PP_HardwareAcceleration acceleration,
                     uint32_t min_picture_count,
                     const CompletionCallback& callback);

  /// Decodes a bitstream buffer. Copies |size| bytes of data from the plugin's
  /// |buffer|. The plugin should wait until the decoder signals completion by
  /// returning PP_OK or by running |callback| before calling Decode() again.
  ///
  /// In general, each bitstream buffer should contain a demuxed bitstream frame
  /// for the selected video codec. For example, H264 decoders expect to receive
  /// one AnnexB NAL unit, including the 4 byte start code prefix, while VP8
  /// decoders expect to receive a bitstream frame without the IVF frame header.
  ///
  /// If the call to Decode() eventually results in a picture, the |decode_id|
  /// parameter is copied into the returned picture. The plugin can use this to
  /// associate decoded pictures with Decode() calls (e.g. to assign timestamps
  /// or frame numbers to pictures.) This value is opaque to the API so the
  /// plugin is free to pass any value.
  ///
  /// @param[in] decode_id An optional value, chosen by the plugin, that can be
  /// used to associate calls to Decode() with decoded pictures returned by
  /// GetPicture().
  /// @param[in] size Buffer size in bytes.
  /// @param[in] buffer Starting address of buffer.
  /// @param[in] callback A <code>CompletionCallback</code> to be called on
  /// completion.
  ///
  /// @return An int32_t containing an error code from <code>pp_errors.h</code>.
  /// Returns PP_ERROR_FAILED if the decoder isn't initialized or if a Flush()
  /// or Reset() call is pending.
  /// Returns PP_ERROR_INPROGRESS if there is another Decode() call pending.
  /// Returns PP_ERROR_NOMEMORY if a bitstream buffer can't be created.
  /// Returns PP_ERROR_ABORTED when Reset() is called while Decode() is pending.
  int32_t Decode(uint32_t decode_id,
                 uint32_t size,
                 const void* buffer,
                 const CompletionCallback& callback);

  /// Gets the next picture from the decoder. The picture is valid after the
  /// decoder signals completion by returning PP_OK or running |callback|. The
  /// plugin can call GetPicture() again after the decoder signals completion.
  /// When the plugin is finished using the picture, it should return it to the
  /// system by calling RecyclePicture().
  ///
  /// @param[in] callback A <code>CompletionCallbackWithOutput</code> to be
  /// called on completion, and on success, to hold the picture descriptor.
  ///
  /// @return An int32_t containing an error code from <code>pp_errors.h</code>.
  /// Returns PP_ERROR_FAILED if the decoder isn't initialized or if a Reset()
  /// call is pending.
  /// Returns PP_ERROR_INPROGRESS if there is another GetPicture() call pending.
  /// Returns PP_ERROR_ABORTED when Reset() is called, or if a call to Flush()
  /// completes while GetPicture() is pending.
  int32_t GetPicture(
      const CompletionCallbackWithOutput<PP_VideoPicture>& callback);

  /// Recycles a picture that the plugin has received from the decoder.
  /// The plugin should call this as soon as it has finished using the texture
  /// so the decoder can decode more pictures.
  ///
  /// @param[in] picture A <code>PP_VideoPicture</code> to return to the
  /// decoder.
  void RecyclePicture(const PP_VideoPicture& picture);

  /// Flushes the decoder. The plugin should call Flush() when it reaches the
  /// end of its video stream in order to stop cleanly. The decoder will run any
  /// pending Decode() call to completion. The plugin should make no further
  /// calls to the decoder other than GetPicture() and RecyclePicture() until
  /// the decoder signals completion by running |callback|. Just before
  /// completion, any pending GetPicture() call will complete by running its
  /// callback with result PP_ERROR_ABORTED to signal that no more pictures are
  /// available. Any pictures held by the plugin remain valid during and after
  /// the flush and should be recycled back to the decoder.
  ///
  /// @param[in] callback A <code>CompletionCallback</code> to be called on
  /// completion.
  ///
  /// @return An int32_t containing an error code from <code>pp_errors.h</code>.
  /// Returns PP_ERROR_FAILED if the decoder isn't initialized.
  int32_t Flush(const CompletionCallback& callback);

  /// Resets the decoder as quickly as possible. The plugin can call Reset() to
  /// skip to another position in the video stream. After Reset() returns, any
  /// pending calls to Decode() and GetPicture()) abort, causing their callbacks
  /// to run with PP_ERROR_ABORTED. The plugin should not make further calls to
  /// the decoder other than RecyclePicture() until the decoder signals
  /// completion by running |callback|. Any pictures held by the plugin remain
  /// valid during and after the reset and should be recycled back to the
  /// decoder.
  ///
  /// @param[in] callback A <code>CompletionCallback</code> to be called on
  /// completion.
  ///
  /// @return An int32_t containing an error code from <code>pp_errors.h</code>.
    /// Returns PP_ERROR_FAILED if the decoder isn't initialized.
int32_t Reset(const CompletionCallback& callback);
};

}  // namespace pp

#endif  // PPAPI_CPP_VIDEO_DECODER_H_
