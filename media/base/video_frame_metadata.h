// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_VIDEO_FRAME_METADATA_H_
#define MEDIA_BASE_VIDEO_FRAME_METADATA_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "media/base/media_export.h"
#include "media/base/video_rotation.h"

namespace media {

class MEDIA_EXPORT VideoFrameMetadata {
 public:
  enum Key {
    // Sources of VideoFrames use this marker to indicate that the associated
    // VideoFrame can be overlayed, case in which its contents do not need to be
    // further composited but displayed directly. Use Get/SetBoolean() for
    // this Key.
    ALLOW_OVERLAY,

    // Video capture begin/end timestamps.  Consumers can use these values for
    // dynamic optimizations, logging stats, etc.  Use Get/SetTimeTicks() for
    // these keys.
    CAPTURE_BEGIN_TIME,
    CAPTURE_END_TIME,

    // Indicates that this frame must be copied to a new texture before use,
    // rather than being used directly. Specifically this is required for
    // WebView because of limitations about sharing surface textures between GL
    // contexts.
    COPY_REQUIRED,

    // Indicates if the current frame is the End of its current Stream. Use
    // Get/SetBoolean() for this Key.
    END_OF_STREAM,

    // The estimated duration of this frame (i.e., the amount of time between
    // the media timestamp of this frame and the next).  Note that this is not
    // the same information provided by FRAME_RATE as the FRAME_DURATION can
    // vary unpredictably for every frame.  Consumers can use this to optimize
    // playback scheduling, make encoding quality decisions, and/or compute
    // frame-level resource utilization stats.  Use Get/SetTimeDelta() for this
    // key.
    FRAME_DURATION,

    // Represents either the fixed frame rate, or the maximum frame rate to
    // expect from a variable-rate source.  This value generally remains the
    // same for all frames in the same session.  Use Get/SetDouble() for this
    // key.
    FRAME_RATE,

    // This is a boolean that signals that the video capture engine detects
    // interactive content. One possible optimization that this signal can help
    // with is remote content: adjusting end-to-end latency down to help the
    // user better coordinate their actions.
    //
    // Use Get/SetBoolean for this key.
    INTERACTIVE_CONTENT,

    // This field represents the local time at which either: 1) the frame was
    // generated, if it was done so locally; or 2) the targeted play-out time
    // of the frame, if it was generated from a remote source. This value is NOT
    // a high-resolution timestamp, and so it should not be used as a
    // presentation time; but, instead, it should be used for buffering playback
    // and for A/V synchronization purposes.
    // Use Get/SetTimeTicks() for this key.
    REFERENCE_TIME,

    // A feedback signal that indicates the fraction of the tolerable maximum
    // amount of resources that were utilized to process this frame.  A producer
    // can check this value after-the-fact, usually via a VideoFrame destruction
    // observer, to determine whether the consumer can handle more or less data
    // volume, and achieve the right quality versus performance trade-off.
    //
    // Use Get/SetDouble() for this key.  Values are interpreted as follows:
    // Less than 0.0 is meaningless and should be ignored.  1.0 indicates a
    // maximum sustainable utilization.  Greater than 1.0 indicates the consumer
    // is likely to stall or drop frames if the data volume is not reduced.
    //
    // Example: In a system that encodes and transmits video frames over the
    // network, this value can be used to indicate whether sufficient CPU
    // is available for encoding and/or sufficient bandwidth is available for
    // transmission over the network.  The maximum of the two utilization
    // measurements would be used as feedback.
    RESOURCE_UTILIZATION,

    // Sources of VideoFrames use this marker to indicate that an instance of
    // VideoFrameExternalResources produced from the associated video frame
    // should use read lock fences.
    READ_LOCK_FENCES_ENABLED,

    // Indicates that the frame is rotated.
    ROTATION,

    // Android only: if set, then this frame is not suitable for overlay, even
    // if ALLOW_OVERLAY is set.  However, it allows us to process the overlay
    // to see if it would have been promoted, if it were backed by a SurfaceView
    // instead.  This lets us figure out when SurfaceViews are appropriate.
    TEXTURE_OWNER,

    // Android only: if set, then this frame's resource would like to be
    // notified about its promotability to an overlay.
    WANTS_PROMOTION_HINT,

    // Windows only: if set, then this frame must be displayed in an overlay
    // rather than being composited into the framebuffer.
    REQUIRE_OVERLAY,

    // Windows only: this video has protected content.
    PROTECTED_VIDEO,

    // Whether this frame was decoded in a power efficient way.
    POWER_EFFICIENT,

    // CompositorFrameMetadata variables associated with this frame. Used for
    // remote debugging.
    // Use Get/SetDouble() for these keys.
    // TODO(crbug.com/832220): Use a customized dictionary value instead of
    // using these keys directly.
    DEVICE_SCALE_FACTOR,
    PAGE_SCALE_FACTOR,
    ROOT_SCROLL_OFFSET_X,
    ROOT_SCROLL_OFFSET_Y,
    TOP_CONTROLS_HEIGHT,
    TOP_CONTROLS_SHOWN_RATIO,

    NUM_KEYS
  };

  VideoFrameMetadata();
  ~VideoFrameMetadata();

  bool HasKey(Key key) const;

  void Clear() { dictionary_.Clear(); }

  // Setters.  Overwrites existing value, if present.
  void SetBoolean(Key key, bool value);
  void SetInteger(Key key, int value);
  void SetDouble(Key key, double value);
  void SetRotation(Key key, VideoRotation value);
  void SetString(Key key, const std::string& value);
  void SetTimeDelta(Key key, const base::TimeDelta& value);
  void SetTimeTicks(Key key, const base::TimeTicks& value);
  void SetValue(Key key, std::unique_ptr<base::Value> value);

  // Getters.  Returns true if |key| is present, and its value has been set.
  bool GetBoolean(Key key, bool* value) const WARN_UNUSED_RESULT;
  bool GetInteger(Key key, int* value) const WARN_UNUSED_RESULT;
  bool GetDouble(Key key, double* value) const WARN_UNUSED_RESULT;
  bool GetRotation(Key key, VideoRotation* value) const WARN_UNUSED_RESULT;
  bool GetString(Key key, std::string* value) const WARN_UNUSED_RESULT;
  bool GetTimeDelta(Key key, base::TimeDelta* value) const WARN_UNUSED_RESULT;
  bool GetTimeTicks(Key key, base::TimeTicks* value) const WARN_UNUSED_RESULT;

  // Returns null if |key| was not present.
  const base::Value* GetValue(Key key) const WARN_UNUSED_RESULT;

  // Convenience method that returns true if |key| exists and is set to true.
  bool IsTrue(Key key) const WARN_UNUSED_RESULT;

  // For serialization.
  std::unique_ptr<base::DictionaryValue> CopyInternalValues() const;
  void MergeInternalValuesFrom(const base::Value& in);
  const base::Value& GetInternalValues() const { return dictionary_; };

  // Merges internal values from |metadata_source|.
  void MergeMetadataFrom(const VideoFrameMetadata* metadata_source);

 private:
  const base::Value* GetBinaryValue(Key key) const;

  base::DictionaryValue dictionary_;

  DISALLOW_COPY_AND_ASSIGN(VideoFrameMetadata);
};

}  // namespace media

#endif  // MEDIA_BASE_VIDEO_FRAME_METADATA_H_
