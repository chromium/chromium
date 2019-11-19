/*
 * Copyright (C) 2011 Google Inc.  All rights reserved.
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

#include "third_party/blink/renderer/core/loader/text_track_loader.h"

#include "services/network/public/mojom/fetch_api.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/raw_resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"

namespace blink {

TextTrackLoader::TextTrackLoader(TextTrackLoaderClient& client,
                                 Document& document)
    : client_(client),
      document_(document),
      cue_load_timer_(document.GetTaskRunner(TaskType::kNetworking),
                      this,
                      &TextTrackLoader::CueLoadTimerFired),
      state_(kLoading),
      new_cues_available_(false) {}

TextTrackLoader::~TextTrackLoader() = default;

void TextTrackLoader::CueLoadTimerFired(TimerBase* timer) {
  DCHECK_EQ(timer, &cue_load_timer_);

  if (new_cues_available_) {
    new_cues_available_ = false;
    client_->NewCuesAvailable(this);
  }

  if (state_ >= kFinished)
    client_->CueLoadingCompleted(this, state_ == kFailed);
}

void TextTrackLoader::CancelLoad() {
  ClearResource();
}

void TextTrackLoader::DataReceived(Resource* resource,
                                   const char* data,
                                   size_t length) {
  DCHECK_EQ(GetResource(), resource);

  if (state_ == kFailed)
    return;

  if (!cue_parser_) {
    cue_parser_ = MakeGarbageCollected<VTTParser, VTTParserClient*, Document&>(
        this, GetDocument());
  }

  cue_parser_->ParseBytes(data, length);
}

void TextTrackLoader::NotifyFinished(Resource* resource) {
  DCHECK_EQ(GetResource(), resource);
  if (cue_parser_)
    cue_parser_->Flush();

  if (state_ != kFailed) {
    if (resource->ErrorOccurred() || !cue_parser_)
      state_ = kFailed;
    else
      state_ = kFinished;
  }

  if (!cue_load_timer_.IsActive())
    cue_load_timer_.StartOneShot(base::TimeDelta(), FROM_HERE);

  CancelLoad();
}

bool TextTrackLoader::Load(const KURL& url,
                           CrossOriginAttributeValue cross_origin) {
  CancelLoad();

  ResourceLoaderOptions options;
  options.initiator_info.name = fetch_initiator_type_names::kTrack;

  // Let |request| be the result of creating a potential-CORS request
  // given |URL|, "track", and |corsAttributeState|, and with the same-origin
  // fallback flag set.
  FetchParameters cue_fetch_params(ResourceRequest(url), options);

  if (cross_origin == kCrossOriginAttributeNotSet) {
    cue_fetch_params.MutableResourceRequest().SetMode(
        network::mojom::RequestMode::kSameOrigin);
  } else {
    cue_fetch_params.SetCrossOriginAccessControl(
        GetDocument().GetSecurityOrigin(), cross_origin);
  }

  ResourceFetcher* fetcher = GetDocument().Fetcher();
  return RawResource::FetchTextTrack(cue_fetch_params, fetcher, this);
}

void TextTrackLoader::NewCuesParsed() {
  if (cue_load_timer_.IsActive())
    return;

  new_cues_available_ = true;
  cue_load_timer_.StartOneShot(base::TimeDelta(), FROM_HERE);
}

void TextTrackLoader::FileFailedToParse() {
  state_ = kFailed;

  if (!cue_load_timer_.IsActive())
    cue_load_timer_.StartOneShot(base::TimeDelta(), FROM_HERE);

  CancelLoad();
}

void TextTrackLoader::GetNewCues(
    HeapVector<Member<TextTrackCue>>& output_cues) {
  DCHECK(cue_parser_);
  if (cue_parser_)
    cue_parser_->GetNewCues(output_cues);
}

void TextTrackLoader::GetNewStyleSheets(
    HeapVector<Member<CSSStyleSheet>>& output_sheets) {
  DCHECK(cue_parser_);
  if (cue_parser_)
    cue_parser_->GetNewStyleSheets(output_sheets);
}

void TextTrackLoader::Trace(blink::Visitor* visitor) {
  visitor->Trace(client_);
  visitor->Trace(cue_parser_);
  visitor->Trace(document_);
  RawResourceClient::Trace(visitor);
  VTTParserClient::Trace(visitor);
}

}  // namespace blink
