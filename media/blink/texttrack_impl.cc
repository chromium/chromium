// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/blink/texttrack_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "media/base/bind_to_current_loop.h"
#include "media/blink/webinbandtexttrack_impl.h"
#include "third_party/blink/public/platform/web_inband_text_track_client.h"
#include "third_party/blink/public/platform/web_media_player_client.h"

namespace media {

TextTrackImpl::TextTrackImpl(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    blink::WebMediaPlayerClient* client,
    std::unique_ptr<WebInbandTextTrackImpl> text_track)
    : task_runner_(task_runner),
      client_(client),
      text_track_(std::move(text_track)) {
  client_->AddTextTrack(text_track_.get());
}

TextTrackImpl::~TextTrackImpl() {
  task_runner_->PostTask(FROM_HERE,
                         base::Bind(&TextTrackImpl::OnRemoveTrack, client_,
                                    base::Passed(&text_track_)));
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
  if (blink::WebInbandTextTrackClient* client = text_track->Client()) {
    client->AddWebVTTCue(start.InSecondsF(), end.InSecondsF(),
                         blink::WebString::FromUTF8(id),
                         blink::WebString::FromUTF8(content),
                         blink::WebString::FromUTF8(settings));
  }
}

void TextTrackImpl::OnRemoveTrack(
    blink::WebMediaPlayerClient* client,
    std::unique_ptr<WebInbandTextTrackImpl> text_track) {
  if (text_track->Client())
    client->RemoveTextTrack(text_track.get());
}

}  // namespace media
