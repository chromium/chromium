// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_LOOPBACK_REFERENCE_MANAGER_H_
#define SERVICES_AUDIO_LOOPBACK_REFERENCE_MANAGER_H_

#include <string>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "media/audio/audio_manager.h"
#include "services/audio/reference_signal_provider.h"

namespace audio {

class LoopbackReferenceManagerCore;
class LoopbackReferenceStreamIdProvider;

// Singleton in the AudioService.
//
// 1) Owns a single system loopback stream and manages its lifetime. It is
// created/deleted on demand when listeners are added/removed.
//
// 2) Produces ReferenceSignalProviders which subscribe to the loopback stream
// and provide its data as the reference signal.
//
// TODO(crbug.com/412581642): Add tests.
class LoopbackReferenceManager : public ReferenceSignalProviderFactory {
 public:
  explicit LoopbackReferenceManager(media::AudioManager* audio_manager);
  LoopbackReferenceManager(const LoopbackReferenceManager&) = delete;
  LoopbackReferenceManager& operator=(const LoopbackReferenceManager&) = delete;
  ~LoopbackReferenceManager() override;

  // ReferenceSignalProviderFactory implementation
  std::unique_ptr<ReferenceSignalProvider> GetReferenceSignalProvider() final;

 private:
  void OnCoreError();

  SEQUENCE_CHECKER(owning_sequence_);
  const raw_ptr<media::AudioManager> audio_manager_;
  const std::unique_ptr<LoopbackReferenceStreamIdProvider> stream_id_provider_;
  std::unique_ptr<LoopbackReferenceManagerCore> core_;
  base::WeakPtrFactory<LoopbackReferenceManager> weak_ptr_factory_{this};
};

}  // namespace audio

#endif  // SERVICES_AUDIO_LOOPBACK_REFERENCE_MANAGER_H_
