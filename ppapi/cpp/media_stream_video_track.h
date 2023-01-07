// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_MEDIA_STREAM_VIDEO_TRACK_H_
#define PPAPI_CPP_MEDIA_STREAM_VIDEO_TRACK_H_

#include <stdint.h>

#include <string>

#include "ppapi/c/ppb_media_stream_video_track.h"
#include "ppapi/cpp/resource.h"
#include "ppapi/cpp/var.h"

/// @file
/// This file defines the <code>MediaStreamVideoTrack</code> interface for a
/// video source resource, which receives video frames from a MediaStream video
/// track in the browser.

namespace pp {

class VideoFrame;
class CompletionCallback;
template <typename T> class CompletionCallbackWithOutput;

/// The <code>MediaStreamVideoTrack</code> class contains methods for
/// receiving video frames from a MediaStream video track in the browser.
class MediaStreamVideoTrack : public Resource {
 public:
  /// Default constructor for creating an is_null()
  /// <code>MediaStreamVideoTrack</code> object.
  MediaStreamVideoTrack();

  /// The copy constructor for <code>MediaStreamVideoTrack</code>.
  ///
  /// @param[in] other A reference to a <code>MediaStreamVideoTrack</code>.
  MediaStreamVideoTrack(const MediaStreamVideoTrack& other);

  /// Constructs a <code>MediaStreamVideoTrack</code> from
  /// a <code>Resource</code>.
  ///
  /// @param[in] resource A <code>PPB_MediaStreamVideoTrack</code> resource.
  explicit MediaStreamVideoTrack(const Resource& resource);

  /// Constructs a <code>MediaStreamVideoTrack</code> that outputs given frames
  /// to a new video track, which will be consumed by Javascript.
  explicit MediaStreamVideoTrack(const InstanceHandle& instance);

  /// A constructor used when you have received a <code>PP_Resource</code> as a
  /// return value that has had 1 ref added for you.
  ///
  /// @param[in] resource A <code>PPB_MediaStreamVideoTrack</code> resource.
  MediaStreamVideoTrack(PassRef, PP_Resource resource);

  ~MediaStreamVideoTrack();

  /// Configures underlying frame buffers for incoming frames.
  /// If the application doesn't want to drop frames, then the
  /// <code>PP_MEDIASTREAMVIDEOTRACK_ATTRIB_BUFFERED_FRAMES</code> should be
  /// chosen such that inter-frame processing time variability won't overrun the
  /// input buffer. If the buffer is overfilled, then frames will be dropped.
  /// The application can detect this by examining the timestamp on returned
  /// frames. If some attributes are not specified, default values will be used
  /// for those unspecified attributes. If <code>Configure()</code> is not
  /// called, default settings will be used.
  /// Example usage from plugin code:
  /// @code
  /// int32_t attribs[] = {
  ///     PP_MEDIASTREAMVIDEOTRACK_ATTRIB_BUFFERED_FRAMES, 4,
  ///     PP_MEDIASTREAMVIDEOTRACK_ATTRIB_NONE};
  /// track.Configure(attribs, callback);
  /// @endcode
  ///
  /// @param[in] attrib_list A list of attribute name-value pairs in which each
  /// attribute is immediately followed by the corresponding desired value.
  /// The list is terminated by
  /// <code>PP_MEDIASTREAMVIDEOTRACK_ATTRIB_NONE</code>.
  /// @param[in] callback A <code>CompletionCallback</code> to be called upon
  /// completion of <code>Configure()</code>.
  ///
  /// @return An int32_t containing a result code from <code>pp_errors.h</code>.
  /// Returns <code>PP_ERROR_INPROGRESS</code> if there is a pending call of
  /// <code>Configure()</code> or <code>GetFrame()</code>, or the plugin
  /// holds some frames which are not recycled with <code>RecycleFrame()</code>.
  /// If an error is returned, all attributes and the underlying buffer will not
  /// be changed.
  int32_t Configure(const int32_t attributes[],
                    const CompletionCallback& callback);

  /// Gets attribute value for a given attribute name.
  ///
  /// @param[in] attrib A <code>PP_MediaStreamVideoTrack_Attrib</code> for
  /// querying.
  /// @param[out] value A int32_t for storing the attribute value.
  ///
  /// @return An int32_t containing a result code from <code>pp_errors.h</code>.
  int32_t GetAttrib(PP_MediaStreamVideoTrack_Attrib attrib,
                    int32_t* value);

  /// Returns the track ID of the underlying MediaStream video track.
  std::string GetId() const;

  /// Checks whether the underlying MediaStream track has ended.
  /// Calls to GetFrame while the track has ended are safe to make and will
  /// complete, but will fail.
  bool HasEnded() const;

  /// Gets the next video frame from the MediaStream track.
  /// If internal processing is slower than the incoming frame rate, new frames
  /// will be dropped from the incoming stream. Once the input buffer is full,
  /// frames will be dropped until <code>RecycleFrame()</code> is called to free
  /// a spot for another frame to be buffered.
  /// If there are no frames in the input buffer,
  /// <code>PP_OK_COMPLETIONPENDING</code> will be returned immediately and the
  /// <code>callback</code> will be called when a new frame is received or some
  /// error happens.
  ///
  /// @param[in] callback A <code>CompletionCallbackWithOutput</code> to be
  /// called upon completion of <code>GetFrame()</code>. If success,
  /// a VideoFrame will be passed into the completion callback function.
  ///
  /// @return An int32_t containing a result code from <code>pp_errors.h</code>.
  /// Returns PP_ERROR_NOMEMORY if <code>max_buffered_frames</code> frames
  /// buffer was not allocated successfully.
  int32_t GetFrame(
      const CompletionCallbackWithOutput<VideoFrame>& callback);

  /// Recycles a frame returned by <code>GetFrame()</code>, so the track can
  /// reuse the underlying buffer of this frame. And the frame will become
  /// invalid. The caller should release all references it holds to
  /// <code>frame</code> and not use it anymore.
  ///
  /// @param[in] frame A VideoFrame returned by <code>GetFrame()</code>.
  ///
  /// @return An int32_t containing a result code from <code>pp_errors.h</code>.
  int32_t RecycleFrame(const VideoFrame& frame);

  /// Closes the MediaStream video track, and disconnects it from video source.
  /// After calling <code>Close()</code>, no new frames will be received.
  void Close();

  // Gets a free frame for output. The frame is allocated by
  // <code>Configure()</code>. The caller should fill it with frame data, and
  // then use |PutFrame()| to send the frame back.
  int32_t GetEmptyFrame(
      const CompletionCallbackWithOutput<VideoFrame>& callback);

  // Sends a frame returned by |GetEmptyFrame()| to the output track.
  // After this function, the |frame| should not be used anymore and the
  // caller should release the reference that it holds.
  int32_t PutFrame(const VideoFrame& frame);

  /// Checks whether a <code>Resource</code> is a MediaStream video track,
  /// to test whether it is appropriate for use with the
  /// <code>MediaStreamVideoTrack</code> constructor.
  ///
  /// @param[in] resource A <code>Resource</code> to test.
  ///
  /// @return True if <code>resource</code> is a MediaStream video track.
  static bool IsMediaStreamVideoTrack(const Resource& resource);
};

}  // namespace pp

#endif  // PPAPI_CPP_MEDIA_STREAM_VIDEO_TRACK_H_
