// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_PRIVATE_VIDEO_FRAME_PRIVATE_H_
#define PPAPI_CPP_PRIVATE_VIDEO_FRAME_PRIVATE_H_

#include <string.h>

#include "ppapi/c/pp_time.h"
#include "ppapi/c/private/pp_video_frame_private.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/pass_ref.h"

/// @file
/// This file defines the struct used to hold a video frame.

namespace pp {

/// The <code>PP_VideoFrame_Private</code> struct represents a video frame.
/// Video sources and destinations use frames to transfer video to and from
/// the browser.
class VideoFrame_Private {
 public:
  /// Default constructor for creating a <code>VideoFrame_Private</code> object.
  VideoFrame_Private();

  /// Constructor that takes an existing <code>PP_VideoFrame_Private</code>
  /// structure. The 'image_data' PP_Resource field in the structure will be
  /// managed by this instance.
  VideoFrame_Private(PassRef, const PP_VideoFrame_Private& pp_video_frame);

  /// Constructor that takes an existing <code>ImageData</code> instance and
  /// a timestamp.
  VideoFrame_Private(const ImageData& image_data, PP_TimeTicks timestamp);

  /// The copy constructor for <code>VideoFrame_Private</code>.
  ///
  /// @param[in] other A reference to a <code>VideoFrame_Private</code>.
  VideoFrame_Private(const VideoFrame_Private& other);

  ~VideoFrame_Private();

  /// The assignment operator for <code>VideoFrame_Private</code>.
  ///
  /// @param[in] other A reference to a <code>VideoFrame_Private</code>.
  VideoFrame_Private& operator=(const VideoFrame_Private& other);

  const PP_VideoFrame_Private& pp_video_frame() const {
    return video_frame_;
  }

  ImageData image_data() const {
    return image_data_;
  }
  void set_image_data(const ImageData& image_data) {
    image_data_ = image_data;
    // The assignment above manages the underlying PP_Resources. Copy the new
    // one into our internal video frame struct.
    video_frame_.image_data = image_data_.pp_resource();
  }

  PP_TimeTicks timestamp() const { return video_frame_.timestamp; }
  void set_timestamp(PP_TimeTicks timestamp) {
    video_frame_.timestamp = timestamp;
  }

 private:
  ImageData image_data_;  // This manages the PP_Resource in video_frame_.
  PP_VideoFrame_Private video_frame_;
};

namespace internal {

// A specialization of CallbackOutputTraits to provide the callback system the
// information on how to handle pp::VideoFrame_Private. This converts
// PP_VideoFrame_Private to pp::VideoFrame_Private when passing to the plugin,
// and specifically manages the PP_Resource embedded in the video_frame_ field.
template<>
struct CallbackOutputTraits<pp::VideoFrame_Private> {
  typedef PP_VideoFrame_Private* APIArgType;
  typedef PP_VideoFrame_Private StorageType;

  static inline APIArgType StorageToAPIArg(StorageType& t) {
    return &t;
  }

  static inline pp::VideoFrame_Private StorageToPluginArg(StorageType& t) {
    return pp::VideoFrame_Private(PASS_REF, t);
  }

  static inline void Initialize(StorageType* t) {
    VideoFrame_Private dummy;
    *t = dummy.pp_video_frame();
  }
};

}  // namespace internal

}  // namespace pp

#endif  // PPAPI_CPP_PRIVATE_VIDEO_FRAME_PRIVATE_H_
