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
class V8UnionAudioSinkOptionsOrString;

class SetSinkIdResolver final : public GarbageCollected<SetSinkIdResolver> {
 public:
  SetSinkIdResolver(ScriptState*,
                    AudioContext&,
                    const V8UnionAudioSinkOptionsOrString&);
  SetSinkIdResolver(const SetSinkIdResolver&) = delete;
  SetSinkIdResolver& operator=(const SetSinkIdResolver&) = delete;
  ~SetSinkIdResolver() = default;

  void Trace(Visitor*) const;

  void Start();

  ScriptPromiseResolver<IDLUndefined>* Resolver();

 private:
  // This callback function is passed to 'AudioDestinationNode::SetSinkId()'.
  // When the device status is okay, 'NotifySetSinkIdIsDone()' gets invoked.
  void OnSetSinkIdComplete(media::OutputDeviceStatus status);

  WeakMember<AudioContext> audio_context_;
  Member<ScriptPromiseResolver<IDLUndefined>> resolver_;
  WebAudioSinkDescriptor sink_descriptor_;
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_SETSINKID_RESOLVER_H_
