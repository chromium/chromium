// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_VIDEO_FRAME_H_
#define PPAPI_CPP_VIDEO_FRAME_H_

#include <stdint.h>

#include "ppapi/c/ppb_video_frame.h"
#include "ppapi/cpp/resource.h"
#include "ppapi/cpp/size.h"

namespace pp {

class VideoFrame : public Resource {
 public:
  /// Default constructor for creating an is_null()
  /// <code>VideoFrame</code> object.
  VideoFrame();

  /// The copy constructor for <code>VideoFrame</code>.
  ///
  /// @param[in] other A reference to a <code>VideoFrame</code>.
  VideoFrame(const VideoFrame& other);

  /// Constructs a <code>VideoFrame</code> from a <code>Resource</code>.
  ///
  /// @param[in] resource A <code>PPB_VideoFrame</code> resource.
  explicit VideoFrame(const Resource& resource);

  /// A constructor used when you have received a <code>PP_Resource</code> as a
  /// return value that has had 1 ref added for you.
  ///
  /// @param[in] resource A <code>PPB_VideoFrame</code> resource.
  VideoFrame(PassRef, PP_Resource resource);

  virtual ~VideoFrame();

  /// Gets the timestamp of the video frame.
  ///
  /// @return A <code>PP_TimeDelta</code> containing the timestamp of the video
  /// frame. Given in seconds since the start of the containing video stream.
  PP_TimeDelta GetTimestamp() const;

  /// Sets the timestamp of the video frame.
  ///
  /// @param[in] timestamp A <code>PP_TimeDelta</code> containing the timestamp
  /// of the video frame. Given in seconds since the start of the containing
  /// video stream.
  void SetTimestamp(PP_TimeDelta timestamp);

  /// Gets the format of the video frame.
  ///
  /// @return A <code>PP_VideoFrame_Format</code> containing the format of the
  /// video frame.
  PP_VideoFrame_Format GetFormat() const;

  /// Gets the size of the video frame.
  ///
  /// @param[out] size A <code>Size</code>.
  ///
  /// @return True on success or false on failure.
  bool GetSize(Size* size) const;

  /// Gets the data buffer for video frame pixels.
  ///
  /// @return A pointer to the beginning of the data buffer.
  void* GetDataBuffer();

  /// Gets the size of data buffer in bytes.
  ///
  /// @return The size of the data buffer in bytes.
  uint32_t GetDataBufferSize() const;
};

}  // namespace pp

#endif  // PPAPI_CPP_VIDEO_FRAME_H_
