// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/renderer.h"
#include "base/logging.h"

namespace media {

Renderer::Renderer() = default;

Renderer::~Renderer() = default;

void Renderer::SetCdm(CdmContext* cdm_context, CdmAttachedCB cdm_attached_cb) {
  DLOG(WARNING) << "CdmContext is not supported.";
  std::move(cdm_attached_cb).Run(false);
}

void Renderer::OnSelectedVideoTracksChanged(
    const std::vector<DemuxerStream*>& enabled_tracks,
    base::OnceClosure change_completed_cb) {
  DLOG(WARNING) << "Track changes are not supported.";
  std::move(change_completed_cb).Run();
}

void Renderer::OnEnabledAudioTracksChanged(
    const std::vector<DemuxerStream*>& enabled_tracks,
    base::OnceClosure change_completed_cb) {
  DLOG(WARNING) << "Track changes are not supported.";
  std::move(change_completed_cb).Run();
}

void Renderer::SetPreservesPitch(bool preserves_pitch) {
  // Not supported by most renderers.
}

void Renderer::SetAutoplayInitiated(bool autoplay_initiated) {
  // Not supported by most renderers.
}

}  // namespace media
