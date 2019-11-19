// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_AUDIO_WORKLET_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_AUDIO_WORKLET_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/workers/worklet.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class AudioWorkletHandler;
class AudioWorkletMessagingProxy;
class BaseAudioContext;
class CrossThreadAudioParamInfo;
class MessagePortChannel;
class SerializedScriptValue;

class MODULES_EXPORT AudioWorklet final : public Worklet {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(AudioWorklet);

 public:
  explicit AudioWorklet(BaseAudioContext*);
  ~AudioWorklet() override = default;

  void CreateProcessor(scoped_refptr<AudioWorkletHandler>,
                       MessagePortChannel,
                       scoped_refptr<SerializedScriptValue> node_options);

  // Invoked by AudioWorkletMessagingProxy. Notifies |context_| when
  // AudioWorkletGlobalScope finishes the first script evaluation and is ready
  // for the worklet operation. Can be used for other post-evaluation tasks
  // in AudioWorklet or BaseAudioContext.
  void NotifyGlobalScopeIsUpdated();

  BaseAudioContext* GetBaseAudioContext() const;

  // Returns |nullptr| if there is no active WorkletGlobalScope().
  AudioWorkletMessagingProxy* GetMessagingProxy();

  const Vector<CrossThreadAudioParamInfo> GetParamInfoListForProcessor(
      const String& name);

  bool IsProcessorRegistered(const String& name);

  // Returns |true| when a AudioWorkletMessagingProxy and a WorkletBackingThread
  // are ready.
  bool IsReady();

  void Trace(blink::Visitor*) override;

 private:
  // Implements Worklet
  bool NeedsToCreateGlobalScope() final;
  WorkletGlobalScopeProxy* CreateGlobalScope() final;

  // To catch the first global scope update and notify the context.
  bool worklet_started_ = false;

  Member<BaseAudioContext> context_;

  DISALLOW_COPY_AND_ASSIGN(AudioWorklet);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBAUDIO_AUDIO_WORKLET_H_
