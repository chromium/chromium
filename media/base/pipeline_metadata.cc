// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/pipeline_metadata.h"

namespace media {

PipelineMetadata::PipelineMetadata() : has_audio(false), has_video(false) {}

PipelineMetadata::~PipelineMetadata() = default;

PipelineMetadata::PipelineMetadata(const PipelineMetadata&) = default;

}  // namespace
