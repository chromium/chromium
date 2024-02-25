// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_MEDIA_SESSION_PUBLIC_CPP_CHAPTER_INFORMATION_H_
#define SERVICES_MEDIA_SESSION_PUBLIC_CPP_CHAPTER_INFORMATION_H_

#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/time/time.h"
#include "build/build_config.h"

#include "services/media_session/public/cpp/media_image.h"

#if BUILDFLAG(IS_ANDROID)

#include <jni.h>

#include "base/android/scoped_java_ref.h"

#endif  // BUILDFLAG(IS_ANDROID)

namespace IPC {
template <class P>
struct ParamTraits;
}  // namespace IPC

namespace ipc_fuzzer {
template <class T>
struct FuzzTraits;
}  // namespace ipc_fuzzer

namespace mojo {
template <typename DataViewType, typename T>
struct StructTraits;
}  // namespace mojo

namespace media_session {

namespace mojom {
class ChapterInformationDataView;
}  // namespace mojom

// Structure representing an ChapterInformation as per the MediaSession API,
// see: https://wicg.github.io/mediasession/#dictdef-chapterinformation
struct COMPONENT_EXPORT(MEDIA_SESSION_BASE_CPP) ChapterInformation {
 public:
  ChapterInformation();
  ChapterInformation(std::u16string title,
                     base::TimeDelta start_time,
                     std::vector<MediaImage> artwork);
  ChapterInformation(const ChapterInformation& other);
  ~ChapterInformation();

  bool operator==(const ChapterInformation& other) const;

  // Returns the title of the video chapter.
  std::u16string title() const;

  // Returns the time the position where this chapter starts.
  base::TimeDelta startTime() const;

  // Returns the images of from this chapter.
  std::vector<MediaImage> artwork() const;

 private:
  friend struct IPC::ParamTraits<media_session::ChapterInformation>;
  friend struct ipc_fuzzer::FuzzTraits<media_session::ChapterInformation>;
  friend struct mojo::StructTraits<mojom::ChapterInformationDataView,
                                   ChapterInformation>;

  // The title of the video chapter.
  std::u16string title_;

  // The time the position where this chapter starts. Should not be negative.
  base::TimeDelta startTime_;

  // The images of from this chapter.
  std::vector<MediaImage> artwork_;
};

}  // namespace media_session

#endif  // SERVICES_MEDIA_SESSION_PUBLIC_CPP_CHAPTER_INFORMATION_H_
