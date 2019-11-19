/*
 * Copyright (C) 2012 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/html/track/inband_text_track.h"

#include "third_party/blink/public/platform/web_inband_text_track.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/track/vtt/vtt_cue.h"

using blink::WebInbandTextTrack;
using blink::WebString;

namespace blink {

namespace {

const AtomicString& TextTrackKindToString(WebInbandTextTrack::Kind kind) {
  switch (kind) {
    case WebInbandTextTrack::kKindSubtitles:
      return TextTrack::SubtitlesKeyword();
    case WebInbandTextTrack::kKindCaptions:
      return TextTrack::CaptionsKeyword();
    case WebInbandTextTrack::kKindDescriptions:
      return TextTrack::DescriptionsKeyword();
    case WebInbandTextTrack::kKindChapters:
      return TextTrack::ChaptersKeyword();
    case WebInbandTextTrack::kKindMetadata:
      return TextTrack::MetadataKeyword();
    case WebInbandTextTrack::kKindNone:
    default:
      break;
  }
  NOTREACHED();
  return TextTrack::SubtitlesKeyword();
}

}  // namespace

InbandTextTrack::InbandTextTrack(WebInbandTextTrack* web_track)
    : TextTrack(TextTrackKindToString(web_track->GetKind()),
                web_track->Label(),
                web_track->Language(),
                web_track->Id(),
                kInBand),
      web_track_(web_track) {
  web_track_->SetClient(this);
}

InbandTextTrack::~InbandTextTrack() {
  if (web_track_)
    web_track_->SetClient(nullptr);
}

void InbandTextTrack::SetTrackList(TextTrackList* track_list) {
  TextTrack::SetTrackList(track_list);
  if (track_list)
    return;

  DCHECK(web_track_);
  web_track_->SetClient(nullptr);
  web_track_ = nullptr;
}

void InbandTextTrack::AddWebVTTCue(double start,
                                   double end,
                                   const WebString& id,
                                   const WebString& content,
                                   const WebString& settings) {
  HTMLMediaElement* owner = MediaElement();
  DCHECK(owner);
  VTTCue* cue = VTTCue::Create(owner->GetDocument(), start, end, content);
  cue->setId(id);
  cue->ParseSettings(nullptr, settings);
  addCue(cue);
}

}  // namespace blink
