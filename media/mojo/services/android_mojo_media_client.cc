// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/android_mojo_media_client.h"

#include <utility>

#include <memory>

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "media/base/android/android_cdm_factory.h"
#include "media/base/audio_decoder.h"
#include "media/base/cdm_factory.h"
#include "media/base/media_log.h"
#include "media/filters/android/media_codec_audio_decoder.h"
#include "media/mojo/mojom/media_drm_storage.mojom.h"
#include "media/mojo/mojom/provision_fetcher.mojom.h"
#include "media/mojo/services/android_mojo_util.h"

using media::android_mojo_util::CreateProvisionFetcher;
using media::android_mojo_util::CreateMediaDrmStorage;

namespace media {

AndroidMojoMediaClient::AndroidMojoMediaClient() {}

AndroidMojoMediaClient::~AndroidMojoMediaClient() {}

// MojoMediaClient overrides.

std::unique_ptr<AudioDecoder> AndroidMojoMediaClient::CreateAudioDecoder(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    std::unique_ptr<MediaLog> media_log) {
  return std::make_unique<MediaCodecAudioDecoder>(task_runner);
}

std::unique_ptr<CdmFactory> AndroidMojoMediaClient::CreateCdmFactory(
    mojom::FrameInterfaceFactory* frame_interfaces) {
  if (!frame_interfaces) {
    NOTREACHED_IN_MIGRATION()
        << "Host interfaces should be provided when using CDM with "
        << "AndroidMojoMediaClient";
    return nullptr;
  }

  return std::make_unique<AndroidCdmFactory>(
      base::BindRepeating(&CreateProvisionFetcher, frame_interfaces),
      base::BindRepeating(&CreateMediaDrmStorage, frame_interfaces));
}

}  // namespace media
