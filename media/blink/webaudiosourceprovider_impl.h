// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BLINK_WEBAUDIOSOURCEPROVIDER_IMPL_H_
#define MEDIA_BLINK_WEBAUDIOSOURCEPROVIDER_IMPL_H_

#include <stddef.h>

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "media/base/audio_renderer_sink.h"
#include "media/blink/media_blink_export.h"
#include "third_party/blink/public/platform/web_audio_source_provider.h"
#include "third_party/blink/public/platform/web_vector.h"

namespace blink {
class WebAudioSourceProviderClient;
}

namespace media {
class MediaLog;

// WebAudioSourceProviderImpl is either one of two things (but not both):
// - a connection between a RestartableAudioRendererSink (the |sink_|) passed in
//   constructor and an AudioRendererSink::RenderCallback passed on Initialize()
//   by means of an internal AudioRendererSink::RenderCallback.
// - a connection between the said AudioRendererSink::RenderCallback and a
//   blink::WebAudioSourceProviderClient passed via setClient() (the |client_|),
//   again using the internal AudioRendererSink::RenderCallback. Blink calls
//   provideInput() periodically to fetch the appropriate data.
//
// In either case, the internal RenderCallback allows for delivering a copy of
// the data if a listener is configured. WASPImpl is also a
// RestartableAudioRendererSink itself in order to be controlled (Play(),
// Pause() etc).
//
// All calls are protected by a lock.
class MEDIA_BLINK_EXPORT WebAudioSourceProviderImpl
    : public blink::WebAudioSourceProvider,
      public SwitchableAudioRendererSink {
 public:
  using CopyAudioCB = base::RepeatingCallback<void(std::unique_ptr<AudioBus>,
                                                   uint32_t frames_delayed,
                                                   int sample_rate)>;

  WebAudioSourceProviderImpl(scoped_refptr<SwitchableAudioRendererSink> sink,
                             MediaLog* media_log);

  // blink::WebAudioSourceProvider implementation.
  void SetClient(blink::WebAudioSourceProviderClient* client) override;
  void ProvideInput(const blink::WebVector<float*>& audio_data,
                    size_t number_of_frames) override;

  // RestartableAudioRendererSink implementation.
  void Initialize(const AudioParameters& params,
                  RenderCallback* renderer) override;
  void Start() override;
  void Stop() override;
  void Play() override;
  void Pause() override;
  bool SetVolume(double volume) override;
  OutputDeviceInfo GetOutputDeviceInfo() override;
  bool IsOptimizedForHardwareParameters() override;
  bool CurrentThreadIsRenderingThread() override;
  void SwitchOutputDevice(const std::string& device_id,
                          OutputDeviceStatusCB callback) override;

  // These methods allow a client to get a copy of the rendered audio.
  void SetCopyAudioCallback(CopyAudioCB callback);
  void ClearCopyAudioCallback();

  int RenderForTesting(AudioBus* audio_bus);

 protected:
  virtual scoped_refptr<SwitchableAudioRendererSink> CreateFallbackSink();
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
  base::Closure set_format_cb_;
  // When set via setClient() it overrides |sink_| for consuming audio.
  blink::WebAudioSourceProviderClient* client_;

  // Where audio ends up unless overridden by |client_|.
  base::Lock sink_lock_;
  scoped_refptr<SwitchableAudioRendererSink> sink_;
  std::unique_ptr<AudioBus> bus_wrapper_;

  // An inner class acting as a T filter where actual data can be tapped.
  class TeeFilter;
  const std::unique_ptr<TeeFilter> tee_filter_;

  MediaLog* const media_log_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<WebAudioSourceProviderImpl> weak_factory_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(WebAudioSourceProviderImpl);
};

}  // namespace media

#endif  // MEDIA_BLINK_WEBAUDIOSOURCEPROVIDER_IMPL_H_
