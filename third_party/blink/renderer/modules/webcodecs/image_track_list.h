// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_IMAGE_TRACK_LIST_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_IMAGE_TRACK_LIST_H_

#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {
class ImageDecoderExternal;
class ImageTrack;

class MODULES_EXPORT ImageTrackList final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit ImageTrackList(ImageDecoderExternal* image_decoder);
  ~ImageTrackList() override;

  // image_track_list.idl implementation.
  uint32_t length() const { return tracks_.size(); }
  ImageTrack* AnonymousIndexedGetter(uint32_t index) const;
  int32_t selectedIndex() const;
  base::Optional<ImageTrack*> selectedTrack() const;

  bool IsEmpty() const { return tracks_.IsEmpty(); }

  // Helper method for ImageDecoder to add tracks. Only one track may be marked
  // as selected at any given time.
  void AddTrack(uint32_t frame_count, int repetition_count, bool selected);

  // Called by ImageTrack entries to update their track selection status.
  void OnTrackSelectionChanged(wtf_size_t index);

  // Disconnects and clears the ImageTrackList. Disconnecting releases all
  // Member<> references held by the ImageTrackList and ImageTrack.
  void Disconnect();

  // GarbageCollected override
  void Trace(Visitor* visitor) const override;

 private:
  Member<ImageDecoderExternal> image_decoder_;
  HeapVector<Member<ImageTrack>> tracks_;
  base::Optional<wtf_size_t> selected_track_id_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_IMAGE_TRACK_LIST_H_
