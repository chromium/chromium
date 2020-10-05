// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEBAUDIOSOURCEPROVIDER_IMPL_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEBAUDIOSOURCEPROVIDER_IMPL_H_

#include <stddef.h>

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "media/base/audio_renderer_sink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_audio_source_provider.h"
#include "third_party/blink/public/platform/web_vector.h"

namespace media {
class MediaLog;
}

namespace blink {

class WebAudioSourceProviderClient;

// WebAudioSourceProviderImpl is either one of two things (but not both):
// - a connection between a RestartableAudioRendererSink (the |sink_|) passed in
//   constructor and an AudioRendererSink::RenderCallback passed on Initialize()
//   by means of an internal AudioRendererSink::RenderCallback.
// - a connection between the said AudioRendererSink::RenderCallback and a
//   WebAudioSourceProviderClient passed via setClient() (the |client_|),
//   again using the internal AudioRendererSink::RenderCallback. Blink calls
//   provideInput() periodically to fetch the appropriate data.
//
// In either case, the internal RenderCallback allows for delivering a copy of
// the data if a listener is configured. WASPImpl is also a
// RestartableAudioRendererSink itself in order to be controlled (Play(),
// Pause() etc).
//
// All calls are protected by a lock.
class BLINK_PLATFORM_EXPORT WebAudioSourceProviderImpl
    : public WebAudioSourceProvider,
      public media::SwitchableAudioRendererSink {
 public:
  using CopyAudioCB =
      base::RepeatingCallback<void(std::unique_ptr<media::AudioBus>,
                                   uint32_t frames_delayed,
                                   int sample_rate)>;

  WebAudioSourceProviderImpl(
      scoped_refptr<media::SwitchableAudioRendererSink> sink,
      media::MediaLog* media_log);

  // WebAudioSourceProvider implementation.
  void SetClient(WebAudioSourceProviderClient* client) override;
  void ProvideInput(const WebVector<float*>& audio_data,
                    size_t number_of_frames) override;

  // RestartableAudioRendererSink implementation.
  void Initialize(const media::AudioParameters& params,
                  RenderCallback* renderer) override;
  void Start() override;
  void Stop() override;
  void Play() override;
  void Pause() override;
  void Flush() override;
  bool SetVolume(double volume) override;
  media::OutputDeviceInfo GetOutputDeviceInfo() override;
  void GetOutputDeviceInfoAsync(OutputDeviceInfoCB info_cb) override;
  bool IsOptimizedForHardwareParameters() override;
  bool CurrentThreadIsRenderingThread() override;
  void SwitchOutputDevice(const std::string& device_id,
                          media::OutputDeviceStatusCB callback) override;

  // These methods allow a client to get a copy of the rendered audio.
  void SetCopyAudioCallback(CopyAudioCB callback);
  void ClearCopyAudioCallback();

  int RenderForTesting(media::AudioBus* audio_bus);

 protected:
  ~WebAudioSourceProviderImpl() override;

 private:
  friend class WebAudioSourceProviderImplTest;

  // Calls setFormat() on |client_| from the Blink renderer thread.
  void OnSetFormat();

  // Used to keep the volume across reconfigurations.
  double volume_;

  // Tracks the current playback state.
  enum PlaybackState { kStopped, kStarted, kPlaying };
  PlaybackState state_;

  // Closure that calls OnSetFormat() on |client_| on the renderer thread.
  base::RepeatingClosure set_format_cb_;

  // When set via setClient() it overrides |sink_| for consuming audio.
  WebAudioSourceProviderClient* client_;

  // Where audio ends up unless overridden by |client_|.
  base::Lock sink_lock_;
  scoped_refptr<media::SwitchableAudioRendererSink> sink_
      GUARDED_BY(sink_lock_);
  std::unique_ptr<media::AudioBus> bus_wrapper_;

  // An inner class acting as a T filter where actual data can be tapped.
  class TeeFilter;
  const std::unique_ptr<TeeFilter> tee_filter_;

  media::MediaLog* const media_log_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<WebAudioSourceProviderImpl> weak_factory_{this};

  DISALLOW_IMPLICIT_CONSTRUCTORS(WebAudioSourceProviderImpl);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEBAUDIOSOURCEPROVIDER_IMPL_H_
