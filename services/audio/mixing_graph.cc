// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/mixing_graph.h"
#include "services/audio/mixing_graph_impl.h"

namespace audio {

// static
std::unique_ptr<MixingGraph> MixingGraph::Create(
    const media::AudioParameters& output_params,
    OnMoreDataCallback on_more_data_cb,
    OnErrorCallback on_error_cb) {
  return std::make_unique<MixingGraphImpl>(
      output_params, std::move(on_more_data_cb), std::move(on_error_cb));
}
}  // namespace audio
