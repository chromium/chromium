// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/media_resource.h"

#include "base/no_destructor.h"
#include "net/cookies/site_for_cookies.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace media {

MediaResource::MediaResource() = default;

MediaResource::~MediaResource() = default;

const MediaUrlParams& MediaResource::GetMediaUrlParams() const {
  NOTREACHED();
  static base::NoDestructor<MediaUrlParams> instance{
      GURL(), net::SiteForCookies(), url::Origin(), false, false, false};
  return *instance;
}

MediaResource::Type MediaResource::GetType() const {
  return STREAM;
}

DemuxerStream* MediaResource::GetFirstStream(DemuxerStream::Type type) {
  const auto& streams = GetAllStreams();
  for (auto* stream : streams) {
    if (stream->type() == type)
      return stream;
  }
  return nullptr;
}

void MediaResource::ForwardDurationChangeToDemuxerHost(
    base::TimeDelta duration) {
  // Only implemented by MediaUrlDemuxer, for the MediaPlayerRendererClient.
  NOTREACHED();
}

}  // namespace media
