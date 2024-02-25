// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/media_session/public/cpp/chapter_information.h"

namespace media_session {
ChapterInformation::ChapterInformation() = default;

ChapterInformation::ChapterInformation(std::u16string title,
                                       base::TimeDelta start_time,
                                       std::vector<MediaImage> artwork)
    : title_(title), startTime_(start_time), artwork_(artwork) {
  DCHECK(start_time >= base::Seconds(0));
}

ChapterInformation::ChapterInformation(const ChapterInformation& other) =
    default;

ChapterInformation::~ChapterInformation() = default;

bool ChapterInformation::operator==(const ChapterInformation& other) const {
  return title_ == other.title_ && startTime_ == other.startTime_ &&
         artwork_ == other.artwork_;
}

base::TimeDelta ChapterInformation::startTime() const {
  return startTime_;
}

std::u16string ChapterInformation::title() const {
  return title_;
}

std::vector<MediaImage> ChapterInformation::artwork() const {
  return artwork_;
}

}  // namespace media_session
