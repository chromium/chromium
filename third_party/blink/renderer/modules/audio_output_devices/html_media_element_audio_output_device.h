// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_AUDIO_OUTPUT_DEVICES_HTML_MEDIA_ELEMENT_AUDIO_OUTPUT_DEVICE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_AUDIO_OUTPUT_DEVICES_HTML_MEDIA_ELEMENT_AUDIO_OUTPUT_DEVICE_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class HTMLMediaElement;
class ScriptState;

class MODULES_EXPORT HTMLMediaElementAudioOutputDevice final
    : public GarbageCollected<HTMLMediaElementAudioOutputDevice>,
      public Supplement<HTMLMediaElement> {
  USING_GARBAGE_COLLECTED_MIXIN(HTMLMediaElementAudioOutputDevice);

 public:
  static const char kSupplementName[];

  HTMLMediaElementAudioOutputDevice();

  void Trace(blink::Visitor*) override;
  static String sinkId(HTMLMediaElement&);
  static ScriptPromise setSinkId(ScriptState*,
                                 HTMLMediaElement&,
                                 const String& sink_id);
  static HTMLMediaElementAudioOutputDevice& From(HTMLMediaElement&);
  void setSinkId(const String&);

 private:
  String sink_id_;
};

}  // namespace blink

#endif
