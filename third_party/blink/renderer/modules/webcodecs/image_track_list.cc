// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/image_track_list.h"

#include "base/logging.h"
#include "third_party/blink/renderer/modules/webcodecs/image_decoder_external.h"
#include "third_party/blink/renderer/modules/webcodecs/image_track.h"

namespace blink {

ImageTrackList::ImageTrackList(ImageDecoderExternal* image_decoder)
    : image_decoder_(image_decoder),
      ready_property_(MakeGarbageCollected<ReadyProperty>(
          image_decoder->GetExecutionContext())) {}

ImageTrackList::~ImageTrackList() = default;

ImageTrack* ImageTrackList::AnonymousIndexedGetter(uint32_t index) const {
  return index >= tracks_.size() ? nullptr : tracks_[index].Get();
}

int32_t ImageTrackList::selectedIndex() const {
  return selected_track_id_.value_or(-1);
}

ImageTrack* ImageTrackList::selectedTrack() const {
  if (!selected_track_id_)
    return nullptr;
  return tracks_[*selected_track_id_].Get();
}

ScriptPromise<IDLUndefined> ImageTrackList::ready(ScriptState* script_state) {
  return ready_property_->Promise(script_state->World());
}

void ImageTrackList::OnTracksReady(DOMException* exception) {
  if (!exception) {
    DCHECK(!IsEmpty());
    ready_property_->ResolveWithUndefined();
  } else {
    DCHECK(IsEmpty());
    ready_property_->Reject(exception);
  }
}

void ImageTrackList::AddTrack(uint32_t frame_count,
                              int repetition_count,
                              bool selected) {
  if (selected) {
    DCHECK(!selected_track_id_.has_value());
    selected_track_id_ = tracks_.size();
  }

  tracks_.push_back(MakeGarbageCollected<ImageTrack>(
      this, tracks_.size(), frame_count, repetition_count, selected));
}

void ImageTrackList::OnTrackSelectionChanged(wtf_size_t index) {
  DCHECK(image_decoder_);
  DCHECK_LT(index, tracks_.size());

  if (selected_track_id_)
    tracks_[*selected_track_id_]->set_selected(false);

  if (tracks_[index]->selected())
    selected_track_id_ = index;
  else
    selected_track_id_.reset();

  image_decoder_->UpdateSelectedTrack();
}

void ImageTrackList::Disconnect() {
  for (auto& track : tracks_)
    track->disconnect();
  image_decoder_ = nullptr;
  selected_track_id_.reset();
  tracks_.clear();
}

void ImageTrackList::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  visitor->Trace(image_decoder_);
  visitor->Trace(tracks_);
  visitor->Trace(ready_property_);
}

}  // namespace blink
