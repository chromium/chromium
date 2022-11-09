// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_SETSINKID_RESOLVER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_SETSINKID_RESOLVER_H_

#include "media/base/output_device_info.h"
#include "third_party/blink/public/platform/web_audio_sink_descriptor.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"

namespace blink {

class AudioContext;
class ScriptPromiseResolver;
class V8UnionAudioSinkOptionsOrString;

class SetSinkIdResolver : public ScriptPromiseResolver {
 public:
  static SetSinkIdResolver* Create(ScriptState*,
                                   AudioContext&,
                                   const V8UnionAudioSinkOptionsOrString&);
  SetSinkIdResolver(ScriptState*,
                    AudioContext&,
                    const V8UnionAudioSinkOptionsOrString&);
  SetSinkIdResolver(const SetSinkIdResolver&) = delete;
  SetSinkIdResolver& operator=(const SetSinkIdResolver&) = delete;
  ~SetSinkIdResolver() override = default;

  void Start();

  void Trace(Visitor*) const override;

 private:
  // This callback function is passed to 'AudioDestinationNode::SetSinkId()'.
  // When the device status is okay, 'NotifySetSinkIdIsDone()' gets invoked.
  void OnSetSinkIdComplete(media::OutputDeviceStatus status);

  // This will update 'AudioContext::sink_id_' and dispatch event.
  void NotifySetSinkIdIsDone();

  WeakMember<AudioContext> audio_context_;

  WebAudioSinkDescriptor sink_descriptor_;
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_SETSINKID_RESOLVER_H_
