// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_MEDIA_CONTROLS_TEXT_TRACK_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_MEDIA_CONTROLS_TEXT_TRACK_MANAGER_H_

#include "base/macros.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class HTMLMediaElement;
class TextTrack;

class MODULES_EXPORT MediaControlsTextTrackManager
    : public GarbageCollected<MediaControlsTextTrackManager> {
 public:
  explicit MediaControlsTextTrackManager(HTMLMediaElement&);

  // Returns the label for the track when a valid track is passed in and "Off"
  // when the parameter is null.
  String GetTextTrackLabel(TextTrack*) const;
  void ShowTextTrackAtIndex(unsigned);
  void DisableShowingTextTracks();

  virtual void Trace(Visitor*);

 private:
  Member<HTMLMediaElement> media_element_;

  DISALLOW_COPY_AND_ASSIGN(MediaControlsTextTrackManager);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_CONTROLS_MEDIA_CONTROLS_TEXT_TRACK_MANAGER_H_
