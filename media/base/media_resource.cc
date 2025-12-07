// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/media_resource.h"

#include "base/no_destructor.h"
#include "net/cookies/site_for_cookies.h"
#include "net/storage_access_api/status.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace media {

MediaResource::MediaResource() = default;

MediaResource::~MediaResource() = default;

DemuxerStream* MediaResource::GetFirstStream(DemuxerStream::Type type) {
  const auto& streams = GetAllStreams();
  for (media::DemuxerStream* stream : streams) {
    if (stream->type() == type)
      return stream;
  }
  return nullptr;
}

}  // namespace media
