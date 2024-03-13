// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_AUDIO_AUDIO_RENDERER_MIXER_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_AUDIO_AUDIO_RENDERER_MIXER_MANAGER_H_

#include <bitset>
#include <map>
#include <memory>
#include <string>

#include "base/check_op.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/synchronization/lock.h"
#include "base/unguessable_token.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_sink_parameters.h"
#include "media/base/audio_latency.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_renderer_mixer_pool.h"
#include "media/base/output_device_info.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/platform/web_common.h"

namespace media {
class AudioRendererMixer;
class AudioRendererMixerInput;
class AudioRendererSink;
}  // namespace media

namespace blink {

// Manages sharing of an AudioRendererMixer among AudioRendererMixerInputs based
// on their AudioParameters configuration.  Inputs with the same AudioParameters
// configuration will share a mixer while a new AudioRendererMixer will be
// lazily created if one with the exact AudioParameters does not exist. When an
// AudioRendererMixer is returned by AudioRendererMixerInput, it will be deleted
// if its only other reference is held by AudioRendererMixerManager.
//
// There should only be one instance of AudioRendererMixerManager per render
// thread.
class BLINK_MODULES_EXPORT AudioRendererMixerManager final
    : public media::AudioRendererMixerPool {
 public:
  // Callback which will be used to create sinks. See AudioDeviceFactory for
  // more details on the parameters.
  using CreateSinkCB =
      base::RepeatingCallback<scoped_refptr<media::AudioRendererSink>(
          const blink::LocalFrameToken& source_frame_token,
          const media::AudioSinkParameters& params)>;

  explicit AudioRendererMixerManager(CreateSinkCB create_sink_cb);
  ~AudioRendererMixerManager() final;

  AudioRendererMixerManager(const AudioRendererMixerManager&) = delete;
  AudioRendererMixerManager& operator=(const AudioRendererMixerManager&) =
      delete;

  // Creates an AudioRendererMixerInput with the proper callbacks necessary to
  // retrieve an AudioRendererMixer instance from AudioRendererMixerManager.
  // |source_frame_token| refers to the RenderFrame containing the entity
  // rendering the audio.  Caller must ensure AudioRendererMixerManager outlives
  // the returned input. |device_id| and |session_id| identify the output
  // device to use. If |device_id| is empty and |session_id| is nonzero,
  // output device associated with the opened input device designated by
  // |session_id| is used. Otherwise, |session_id| is ignored.
  scoped_refptr<media::AudioRendererMixerInput> CreateInput(
      const blink::LocalFrameToken& source_frame_token,
      const base::UnguessableToken& session_id,
      const std::string& device_id,
      media::AudioLatency::Type latency);

  // media::AudioRendererMixerPool implementation. The rest of the
  // implementation is kept private (see comment below).
  void ReturnMixer(media::AudioRendererMixer* mixer) final;

  // media::AudioRendererMixerPool look-alikes, with strongly typed tokens.
  // Clients in blink/ should use these functions.
  media::AudioRendererMixer* GetMixer(
      const blink::LocalFrameToken& source_frame_token,
      const media::AudioParameters& input_params,
      media::AudioLatency::Type latency,
      const media::OutputDeviceInfo& sink_info,
      scoped_refptr<media::AudioRendererSink> sink);
  scoped_refptr<media::AudioRendererSink> GetSink(
      const blink::LocalFrameToken& source_frame_token,
      const std::string& device_id);

 private:
  friend class AudioRendererMixerManagerTest;

  // media::AudioRendererMixerPool implementation. This interface faces
  // code in media/ which uses untyped tokens, and is kept private so that
  // blink/ clients prefer to use the strongly-typed-token variants.
  media::AudioRendererMixer* GetMixer(
      const base::UnguessableToken& source_frame_token,
      const media::AudioParameters& input_params,
      media::AudioLatency::Type latency,
      const media::OutputDeviceInfo& sink_info,
      scoped_refptr<media::AudioRendererSink> sink) final;
  scoped_refptr<media::AudioRendererSink> GetSink(
      const base::UnguessableToken& source_frame_token,
      const std::string& device_id) final;

  // Define a key so that only those AudioRendererMixerInputs from the same
  // RenderView, AudioParameters and output device can be mixed together.
  struct MixerKey {
    MixerKey(const blink::LocalFrameToken& source_frame_token,
             const media::AudioParameters& params,
             media::AudioLatency::Type latency,
             const std::string& device_id);
    MixerKey(const MixerKey& other);
    ~MixerKey();
    blink::LocalFrameToken source_frame_token;
    media::AudioParameters params;
    media::AudioLatency::Type latency;
    std::string device_id;
  };

  // Custom compare operator for the AudioRendererMixerMap.  Allows reuse of
  // mixers where only irrelevant keys mismatch.
  struct MixerKeyCompare {
    bool operator()(const MixerKey& a, const MixerKey& b) const {
      if (a.source_frame_token != b.source_frame_token)
        return a.source_frame_token < b.source_frame_token;
      if (a.params.channels() != b.params.channels())
        return a.params.channels() < b.params.channels();

      if (a.latency != b.latency)
        return a.latency < b.latency;

      // TODO(olka) add buffer duration comparison for kLatencyExactMS when
      // adding support for it.
      DCHECK_NE(media::AudioLatency::Type::kExactMS, a.latency);

      // Ignore format(), and frames_per_buffer(), these parameters do not
      // affect mixer reuse.  All AudioRendererMixer units disable FIFO, so
      // frames_per_buffer() can be safely ignored.
      if (a.params.channel_layout() != b.params.channel_layout())
        return a.params.channel_layout() < b.params.channel_layout();
      if (a.params.effects() != b.params.effects())
        return a.params.effects() < b.params.effects();

      if (media::AudioDeviceDescription::IsDefaultDevice(a.device_id) &&
          media::AudioDeviceDescription::IsDefaultDevice(b.device_id)) {
        // Both device IDs represent the same default device => do not compare
        // them.
        return false;
      }

      return a.device_id < b.device_id;
    }
  };

  const CreateSinkCB create_sink_cb_;

  // Map of MixerKey to <AudioRendererMixer, Count>.  Count allows
  // AudioRendererMixerManager to keep track explicitly (v.s. RefCounted which
  // is implicit) of the number of outstanding AudioRendererMixers.
  struct AudioRendererMixerReference {
    raw_ptr<media::AudioRendererMixer, DanglingUntriaged> mixer;
    size_t ref_count;
  };

  using AudioRendererMixerMap =
      base::flat_map<MixerKey, AudioRendererMixerReference, MixerKeyCompare>;

  // Active mixers.
  AudioRendererMixerMap mixers_;
  base::Lock mixers_lock_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIA_AUDIO_AUDIO_RENDERER_MIXER_MANAGER_H_
