// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/mp4/bitstream_converter.h"

namespace media {
namespace mp4 {

BitstreamConverter::AnalysisResult::AnalysisResult() {}

BitstreamConverter::AnalysisResult::AnalysisResult(const AnalysisResult& other)
    : is_conformant(other.is_conformant), is_keyframe(other.is_keyframe) {}

BitstreamConverter::AnalysisResult::~AnalysisResult() = default;

BitstreamConverter::~BitstreamConverter() = default;

}  // namespace mp4
}  // namespace media
