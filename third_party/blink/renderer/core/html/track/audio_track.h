// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_TRACK_AUDIO_TRACK_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_TRACK_AUDIO_TRACK_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/track/track_base.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class CORE_EXPORT AudioTrack final : public ScriptWrappable, public TrackBase {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(AudioTrack);

 public:
  AudioTrack(const String& id,
             const AtomicString& kind,
             const AtomicString& label,
             const AtomicString& language,
             bool enabled);
  ~AudioTrack() override;
  void Trace(Visitor*) override;

  bool enabled() const { return enabled_; }
  void setEnabled(bool);

  // Valid kind keywords.
  static const AtomicString& AlternativeKeyword();
  static const AtomicString& DescriptionsKeyword();
  static const AtomicString& MainKeyword();
  static const AtomicString& MainDescriptionsKeyword();
  static const AtomicString& TranslationKeyword();
  static const AtomicString& CommentaryKeyword();

  static bool IsValidKindKeyword(const String&);

 private:
  bool enabled_;
};

DEFINE_TRACK_TYPE_CASTS(AudioTrack, WebMediaPlayer::kAudioTrack);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_TRACK_AUDIO_TRACK_H_
