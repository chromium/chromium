// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_OWNING_AUDIO_MANAGER_ACCESSOR_H_
#define SERVICES_AUDIO_OWNING_AUDIO_MANAGER_ACCESSOR_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/threading/thread_checker.h"
#include "build/build_config.h"
#include "services/audio/service.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/scoped_com_initializer.h"
#endif

namespace media {
class AudioLogFactory;
class AudioManager;
class AudioThread;
}  // namespace media

namespace audio {

// Lazily creates AudioManager using provided factory callback and controls its
// lifetime. Threading model of a created AudioManager is enforced: its main
// thread is the thread OwningAudioManagerAccessor lives on.
class OwningAudioManagerAccessor : public Service::AudioManagerAccessor {
 public:
  using AudioManagerFactoryCallback =
      base::OnceCallback<std::unique_ptr<media::AudioManager>(
          std::unique_ptr<media::AudioThread>,
          media::AudioLogFactory* audio_log_factory)>;

  explicit OwningAudioManagerAccessor(
      AudioManagerFactoryCallback audio_manager_factory_cb);

  OwningAudioManagerAccessor(const OwningAudioManagerAccessor&) = delete;
  OwningAudioManagerAccessor& operator=(const OwningAudioManagerAccessor&) =
      delete;

  ~OwningAudioManagerAccessor() override;

  media::AudioManager* GetAudioManager() final;
  void SetAudioLogFactory(media::AudioLogFactory* factory) final;
  void Shutdown() final;

 private:
#if BUILDFLAG(IS_WIN)
  // Required to access CoreAudio.
  base::win::ScopedCOMInitializer com_initializer_{
      base::win::ScopedCOMInitializer::kMTA};
#endif
  AudioManagerFactoryCallback audio_manager_factory_cb_;
  std::unique_ptr<media::AudioManager> audio_manager_;
  raw_ptr<media::AudioLogFactory, DanglingUntriaged> log_factory_ =
      nullptr;  // not owned.

  THREAD_CHECKER(thread_checker_);
};

}  // namespace audio

#endif  // SERVICES_AUDIO_OWNING_AUDIO_MANAGER_ACCESSOR_H_
