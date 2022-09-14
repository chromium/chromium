// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/text_cue.h"

namespace media {

TextCue::TextCue(const base::TimeDelta& timestamp,
                 const base::TimeDelta& duration,
                 const std::string& id,
                 const std::string& settings,
                 const std::string& text)
    : timestamp_(timestamp),
      duration_(duration),
      id_(id),
      settings_(settings),
      text_(text) {
}

TextCue::~TextCue() = default;

}  // namespace media
