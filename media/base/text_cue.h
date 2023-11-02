// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_TEXT_CUE_H_
#define MEDIA_BASE_TEXT_CUE_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "media/base/media_export.h"

namespace media {

// A text buffer to carry the components of a text track cue.
class MEDIA_EXPORT TextCue
    : public base::RefCountedThreadSafe<TextCue> {
 public:
  TextCue() = delete;

  TextCue(const base::TimeDelta& timestamp,
          const base::TimeDelta& duration,
          const std::string& id,
          const std::string& settings,
          const std::string& text);

  TextCue(const TextCue&) = delete;
  TextCue& operator=(const TextCue&) = delete;

  // Access to constructor parameters.
  base::TimeDelta timestamp() const { return timestamp_; }
  base::TimeDelta duration() const { return duration_; }
  const std::string& id() const { return id_; }
  const std::string& settings() const { return settings_; }
  const std::string& text() const { return text_; }

 private:
  friend class base::RefCountedThreadSafe<TextCue>;
  ~TextCue();

  base::TimeDelta timestamp_;
  base::TimeDelta duration_;
  std::string id_;
  std::string settings_;
  std::string text_;
};

}  // namespace media

#endif  // MEDIA_BASE_TEXT_CUE_H_
