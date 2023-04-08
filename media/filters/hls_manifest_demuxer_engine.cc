// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/hls_manifest_demuxer_engine.h"

#include <vector>

#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "media/base/media_log.h"
#include "media/base/media_track.h"
#include "media/base/pipeline_status.h"
#include "media/filters/manifest_demuxer.h"
#include "media/formats/hls/audio_rendition.h"
#include "media/formats/hls/media_playlist.h"
#include "media/formats/hls/multivariant_playlist.h"
#include "media/formats/hls/types.h"
#include "media/formats/hls/variant_stream.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace media {

HlsManifestDemuxerEngine::~HlsManifestDemuxerEngine() = default;
HlsManifestDemuxerEngine::HlsManifestDemuxerEngine() = default;

std::string HlsManifestDemuxerEngine::GetName() const {
  return "HlsManifestDemuxer";
}

void HlsManifestDemuxerEngine::Initialize(ManifestDemuxerEngineHost* host,
                                          PipelineStatusCallback status_cb) {
  std::move(status_cb).Run(PIPELINE_ERROR_INVALID_STATE);
}

void HlsManifestDemuxerEngine::OnTimeUpdate(base::TimeDelta time,
                                            double playback_rate,
                                            ManifestDemuxer::DelayCallback cb) {
  std::move(cb).Run(kNoTimestamp);
}

bool HlsManifestDemuxerEngine::Seek(base::TimeDelta time) {
  // TODO(crbug/1266991)
  NOTIMPLEMENTED();
  return true;
}

void HlsManifestDemuxerEngine::StartWaitingForSeek() {
  // TODO(crbug/1266991)
  NOTIMPLEMENTED();
}

void HlsManifestDemuxerEngine::AbortPendingReads() {
  // TODO(crbug/1266991)
  NOTIMPLEMENTED();
}

bool HlsManifestDemuxerEngine::IsSeekable() {
  NOTIMPLEMENTED();
  return true;
}

int64_t HlsManifestDemuxerEngine::GetMemoryUsage() const {
  return 0;
}

void HlsManifestDemuxerEngine::Stop() {
  // TODO(crbug/1266991)
  NOTIMPLEMENTED();
}

}  // namespace media
