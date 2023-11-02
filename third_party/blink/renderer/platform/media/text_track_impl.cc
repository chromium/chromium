// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/media/text_track_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "media/base/bind_to_current_loop.h"
#include "third_party/blink/public/platform/web_inband_text_track_client.h"
#include "third_party/blink/public/platform/web_media_player_client.h"
#include "third_party/blink/renderer/platform/media/web_inband_text_track_impl.h"

namespace blink {

TextTrackImpl::TextTrackImpl(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    WebMediaPlayerClient* client,
    std::unique_ptr<WebInbandTextTrackImpl> text_track)
    : task_runner_(task_runner),
      client_(client),
      text_track_(std::move(text_track)) {
  client_->AddTextTrack(text_track_.get());
}

TextTrackImpl::~TextTrackImpl() {
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&TextTrackImpl::OnRemoveTrack, client_,
                                        std::move(text_track_)));
}

void TextTrackImpl::addWebVTTCue(base::TimeDelta start,
                                 base::TimeDelta end,
                                 const std::string& id,
                                 const std::string& content,
                                 const std::string& settings) {
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&TextTrackImpl::OnAddCue, text_track_.get(),
                                start, end, id, content, settings));
}

void TextTrackImpl::OnAddCue(WebInbandTextTrackImpl* text_track,
                             base::TimeDelta start,
                             base::TimeDelta end,
                             const std::string& id,
                             const std::string& content,
                             const std::string& settings) {
  if (WebInbandTextTrackClient* client = text_track->Client()) {
    client->AddWebVTTCue(start.InSecondsF(), end.InSecondsF(),
                         WebString::FromUTF8(id), WebString::FromUTF8(content),
                         WebString::FromUTF8(settings));
  }
}

void TextTrackImpl::OnRemoveTrack(
    WebMediaPlayerClient* client,
    std::unique_ptr<WebInbandTextTrackImpl> text_track) {
  if (text_track->Client())
    client->RemoveTextTrack(text_track.get());
}

}  // namespace blink
