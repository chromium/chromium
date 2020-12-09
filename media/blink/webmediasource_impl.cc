// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/blink/webmediasource_impl.h"

#include "base/guid.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/mime_util.h"
#include "media/base/video_decoder_config.h"
#include "media/blink/websourcebuffer_impl.h"
#include "media/filters/chunk_demuxer.h"
#include "third_party/blink/public/platform/web_string.h"

using ::blink::WebString;
using ::blink::WebMediaSource;

namespace media {

#define STATIC_ASSERT_MATCHING_STATUS_ENUM(webkit_name, chromium_name) \
  static_assert(static_cast<int>(WebMediaSource::webkit_name) == \
                static_cast<int>(ChunkDemuxer::chromium_name),  \
                "mismatching status enum values: " #webkit_name)
STATIC_ASSERT_MATCHING_STATUS_ENUM(kAddStatusOk, kOk);
STATIC_ASSERT_MATCHING_STATUS_ENUM(kAddStatusNotSupported, kNotSupported);
STATIC_ASSERT_MATCHING_STATUS_ENUM(kAddStatusReachedIdLimit, kReachedIdLimit);
#undef STATIC_ASSERT_MATCHING_STATUS_ENUM

WebMediaSourceImpl::WebMediaSourceImpl(ChunkDemuxer* demuxer)
    : demuxer_(demuxer) {
  DCHECK(demuxer_);
}

WebMediaSourceImpl::~WebMediaSourceImpl() = default;

std::unique_ptr<blink::WebSourceBuffer> WebMediaSourceImpl::AddSourceBuffer(
    const blink::WebString& content_type,
    const blink::WebString& codecs,
    WebMediaSource::AddStatus& out_status /* out */) {
  std::string id = base::GenerateGUID();

  out_status = static_cast<WebMediaSource::AddStatus>(
      demuxer_->AddId(id, content_type.Utf8(), codecs.Utf8()));

  if (out_status == WebMediaSource::kAddStatusOk)
    return std::make_unique<WebSourceBufferImpl>(id, demuxer_);

  return nullptr;
}

std::unique_ptr<blink::WebSourceBuffer> WebMediaSourceImpl::AddSourceBuffer(
    std::unique_ptr<AudioDecoderConfig> audio_config,
    WebMediaSource::AddStatus& out_status /* out */) {
  std::string id = base::GenerateGUID();

  out_status = static_cast<WebMediaSource::AddStatus>(
      demuxer_->AddId(id, std::move(audio_config)));

  if (out_status == WebMediaSource::kAddStatusOk)
    return std::make_unique<WebSourceBufferImpl>(id, demuxer_);

  return nullptr;
}

std::unique_ptr<blink::WebSourceBuffer> WebMediaSourceImpl::AddSourceBuffer(
    std::unique_ptr<VideoDecoderConfig> video_config,
    WebMediaSource::AddStatus& out_status /* out */) {
  std::string id = base::GenerateGUID();

  out_status = static_cast<WebMediaSource::AddStatus>(
      demuxer_->AddId(id, std::move(video_config)));

  if (out_status == WebMediaSource::kAddStatusOk)
    return std::make_unique<WebSourceBufferImpl>(id, demuxer_);

  return nullptr;
}

double WebMediaSourceImpl::Duration() {
  return demuxer_->GetDuration();
}

void WebMediaSourceImpl::SetDuration(double new_duration) {
  DCHECK_GE(new_duration, 0);
  demuxer_->SetDuration(new_duration);
}

void WebMediaSourceImpl::MarkEndOfStream(
    WebMediaSource::EndOfStreamStatus status) {
  PipelineStatus pipeline_status = PIPELINE_OK;

  switch (status) {
    case WebMediaSource::kEndOfStreamStatusNoError:
      break;
    case WebMediaSource::kEndOfStreamStatusNetworkError:
      pipeline_status = CHUNK_DEMUXER_ERROR_EOS_STATUS_NETWORK_ERROR;
      break;
    case WebMediaSource::kEndOfStreamStatusDecodeError:
      pipeline_status = CHUNK_DEMUXER_ERROR_EOS_STATUS_DECODE_ERROR;
      break;
  }

  demuxer_->MarkEndOfStream(pipeline_status);
}

void WebMediaSourceImpl::UnmarkEndOfStream() {
  demuxer_->UnmarkEndOfStream();
}

}  // namespace media
