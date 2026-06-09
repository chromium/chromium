// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/public/cpp/sounds/sounds_manager.h"

#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "media/base/audio_codecs.h"
#include "services/audio/public/cpp/sounds/audio_stream_handler.h"

namespace audio {

namespace {

// SoundsManagerImpl ---------------------------------------------------

class SoundsManagerImpl : public SoundsManager {
 public:
  explicit SoundsManagerImpl(StreamFactoryBinder stream_factory_binder)
      : stream_factory_binder_(std::move(stream_factory_binder)) {}

  SoundsManagerImpl(const SoundsManagerImpl&) = delete;
  SoundsManagerImpl& operator=(const SoundsManagerImpl&) = delete;

  ~SoundsManagerImpl() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  // SoundsManager implementation:
  bool Initialize(SoundKey key,
                  int resource_id,
                  media::AudioCodec codec,
                  bool loop) override;
  bool Play(SoundKey key) override;
  bool Stop(SoundKey key) override;
  bool Pause(SoundKey key) override;
  base::TimeDelta GetDuration(SoundKey key) override;

 private:
  AudioStreamHandler* GetHandler(SoundKey key);

  // There's only a handful of sounds, so a vector is sufficient.
  struct StreamEntry {
    SoundKey key;
    std::unique_ptr<AudioStreamHandler> handler;
  };
  std::vector<StreamEntry> handlers_;
  StreamFactoryBinder stream_factory_binder_;
};

bool SoundsManagerImpl::Initialize(SoundKey key,
                                   int resource_id,
                                   media::AudioCodec codec,
                                   bool loop) {
  if (AudioStreamHandler* handler = GetHandler(key)) {
    DCHECK(handler->IsInitialized());
    return true;
  }

  auto handler = std::make_unique<AudioStreamHandler>(stream_factory_binder_,
                                                      resource_id, codec, loop);
  if (!handler->IsInitialized()) {
    LOG(WARNING) << "Can't initialize AudioStreamHandler for key=" << key;
    return false;
  }

  handlers_.push_back({key, std::move(handler)});
  return true;
}

bool SoundsManagerImpl::Play(SoundKey key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  AudioStreamHandler* handler = GetHandler(key);
  return handler && handler->Play();
}

bool SoundsManagerImpl::Stop(SoundKey key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  AudioStreamHandler* handler = GetHandler(key);
  if (!handler) {
    return false;
  }
  handler->Stop();
  return true;
}

bool SoundsManagerImpl::Pause(SoundKey key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  AudioStreamHandler* handler = GetHandler(key);
  if (!handler) {
    return false;
  }
  return handler->Pause();
}

base::TimeDelta SoundsManagerImpl::GetDuration(SoundKey key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  AudioStreamHandler* handler = GetHandler(key);
  return !handler ? base::TimeDelta() : handler->duration();
}

AudioStreamHandler* SoundsManagerImpl::GetHandler(SoundKey key) {
  for (auto& entry : handlers_) {
    if (entry.key == key) {
      return entry.handler.get();
    }
  }
  return nullptr;
}

}  // namespace

// static
std::unique_ptr<SoundsManager> SoundsManager::Create(
    SoundsManager::StreamFactoryBinder stream_factory_binder) {
  return std::make_unique<SoundsManagerImpl>(std::move(stream_factory_binder));
}

SoundsManager::SoundsManager() = default;

SoundsManager::~SoundsManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

}  // namespace audio
