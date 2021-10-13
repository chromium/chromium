// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/mixing_graph.h"

#include "base/notreached.h"
namespace audio {

// static
std::unique_ptr<MixingGraph> MixingGraph::Create(
    const media::AudioParameters& output_params,
    OnMoreDataCallback on_more_data_cb,
    OnErrorCallback on_error_cb) {
  NOTIMPLEMENTED();
  return nullptr;
}

}  // namespace audio
