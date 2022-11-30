// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_MEDIA_SESSION_PUBLIC_CPP_MEDIA_POSITION_H_
#define SERVICES_MEDIA_SESSION_PUBLIC_CPP_MEDIA_POSITION_H_

#include "base/component_export.h"
#include "base/gtest_prod_util.h"
#include "base/time/time.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)

#include <jni.h>

#include "base/android/scoped_java_ref.h"

#endif  // BUILDFLAG(IS_ANDROID)

namespace IPC {
template <class P>
struct ParamTraits;
}

namespace ipc_fuzzer {
template <class T>
struct FuzzTraits;
}  // namespace ipc_fuzzer

namespace mojo {
template <typename DataViewType, typename T>
struct StructTraits;
}

namespace media_session {

namespace mojom {
class MediaPositionDataView;
}

struct COMPONENT_EXPORT(MEDIA_SESSION_BASE_CPP) MediaPosition {
 public:
  MediaPosition();
  MediaPosition(double playback_rate,
                base::TimeDelta duration,
                base::TimeDelta position,
                bool end_of_media);
  ~MediaPosition();

#if BUILDFLAG(IS_ANDROID)
  // Creates a Java MediaPosition instance and returns the JNI ref.
  base::android::ScopedJavaLocalRef<jobject> CreateJavaObject(
      JNIEnv* env) const;
#endif

  // Return the duration of the media.
  base::TimeDelta duration() const;

  // Return the current position of the media.
  base::TimeDelta GetPosition() const;

  // Return the current playback rate of the media.
  double playback_rate() const;

  // Return the time the position state was last updated.
  base::TimeTicks last_updated_time() const;

  // Return whether playback has reached the end of media. This can be true
  // even when GetPosition() < duration(), because the duration is not exact in
  // general.
  bool end_of_media() const { return end_of_media_; }

  // Return the updated position of the media, assuming current time is
  // |time|.
  base::TimeDelta GetPositionAtTime(base::TimeTicks time) const;

  bool operator==(const MediaPosition&) const;
  bool operator!=(const MediaPosition&) const;

  std::string ToString() const;

 private:
  friend struct IPC::ParamTraits<media_session::MediaPosition>;
  friend struct ipc_fuzzer::FuzzTraits<media_session::MediaPosition>;
  friend struct mojo::StructTraits<mojom::MediaPositionDataView, MediaPosition>;
  friend class MediaPositionTest;
  FRIEND_TEST_ALL_PREFIXES(MediaPositionTest, TestPositionUpdated);
  FRIEND_TEST_ALL_PREFIXES(MediaPositionTest, TestPositionUpdatedTwice);
  FRIEND_TEST_ALL_PREFIXES(MediaPositionTest, TestPositionUpdatedPastDuration);
  FRIEND_TEST_ALL_PREFIXES(MediaPositionTest, TestNegativePosition);
  FRIEND_TEST_ALL_PREFIXES(MediaPositionTest,
                           TestPositionUpdatedFasterPlayback);
  FRIEND_TEST_ALL_PREFIXES(MediaPositionTest,
                           TestPositionUpdatedSlowerPlayback);
  FRIEND_TEST_ALL_PREFIXES(MediaPositionTest,
                           TestNotEquals_DifferentEndOfMedia);
  FRIEND_TEST_ALL_PREFIXES(MediaPositionTest, TestEquals_AllSame);
  FRIEND_TEST_ALL_PREFIXES(MediaPositionTest, TestEquals_SameButDifferentTime);
  FRIEND_TEST_ALL_PREFIXES(MediaPositionTest, TestNotEquals_DifferentDuration);
  FRIEND_TEST_ALL_PREFIXES(MediaPositionTest,
                           TestNotEquals_DifferentPlaybackRate);

  // Playback rate of the media.
  double playback_rate_ = 0;

  // Duration of the media.
  base::TimeDelta duration_;

  // Last updated position of the media.
  base::TimeDelta position_;

  // Last time |position_| was updated.
  base::TimeTicks last_updated_time_;

  // Whether playback has reached the end of media.
  bool end_of_media_ = false;
};

}  // namespace media_session

#endif  // SERVICES_MEDIA_SESSION_PUBLIC_CPP_MEDIA_POSITION_H_
