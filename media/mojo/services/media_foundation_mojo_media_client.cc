// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/media_foundation_mojo_media_client.h"

#include "media/base/audio_decoder.h"
#include "media/base/win/mf_helpers.h"
#include "media/cdm/cdm_adapter_factory.h"
#include "media/mojo/services/mojo_cdm_helper.h"

namespace media {

namespace {

std::unique_ptr<media::CdmAuxiliaryHelper> CreateCdmHelper(
    mojom::FrameInterfaceFactory* frame_interfaces) {
  return std::make_unique<media::MojoCdmHelper>(frame_interfaces);
}

}  // namespace

MediaFoundationMojoMediaClient::MediaFoundationMojoMediaClient() {
  DVLOG_FUNC(1);
}

MediaFoundationMojoMediaClient::~MediaFoundationMojoMediaClient() {
  DVLOG_FUNC(1);
}

// MojoMediaClient overrides.

std::unique_ptr<media::CdmFactory>
MediaFoundationMojoMediaClient::CreateCdmFactory(
    mojom::FrameInterfaceFactory* frame_interfaces) {
  DVLOG_FUNC(1);

  // TODO(frankli): consider to use MediaFoundationCdmFactory instead of
  // CdmAdapterFactory.
  return std::make_unique<media::CdmAdapterFactory>(
      base::BindRepeating(&CreateCdmHelper, frame_interfaces));
}

}  // namespace media
