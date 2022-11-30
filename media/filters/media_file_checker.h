// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_MEDIA_FILE_CHECKER_H_
#define MEDIA_FILTERS_MEDIA_FILE_CHECKER_H_

#include "base/files/file.h"
#include "media/base/media_export.h"

namespace base {
class TimeDelta;
}

namespace media {

// This class tries to determine if a file is a valid media file. The entire
// file is not decoded so a positive result from this class does not make the
// file safe to use in the browser process.
class MEDIA_EXPORT MediaFileChecker {
 public:
  explicit MediaFileChecker(base::File file);

  MediaFileChecker(const MediaFileChecker&) = delete;
  MediaFileChecker& operator=(const MediaFileChecker&) = delete;

  ~MediaFileChecker();

  // After opening |file|, up to |check_time| amount of wall-clock time is spent
  // decoding the file. The amount of audio/video data decoded will depend on
  // the bitrate of the file and the speed of the CPU.
  bool Start(base::TimeDelta check_time);

 private:
  base::File file_;
};

}  // namespace media

#endif  // MEDIA_FILTERS_MEDIA_FILE_CHECKER_H_
