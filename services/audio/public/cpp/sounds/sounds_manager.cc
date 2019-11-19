// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/public/cpp/sounds/sounds_manager.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "services/audio/public/cpp/sounds/audio_stream_handler.h"

namespace audio {

namespace {

SoundsManager* g_instance = NULL;
bool g_initialized_for_testing = false;

// SoundsManagerImpl ---------------------------------------------------

class SoundsManagerImpl : public SoundsManager {
 public:
  SoundsManagerImpl(std::unique_ptr<service_manager::Connector> connector)
      : connector_(std::move(connector)) {}
  ~SoundsManagerImpl() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  // SoundsManager implementation:
  bool Initialize(SoundKey key, const base::StringPiece& data) override;
  bool Play(SoundKey key) override;
  bool Stop(SoundKey key) override;
  base::TimeDelta GetDuration(SoundKey key) override;

 private:
  AudioStreamHandler* GetHandler(SoundKey key);

  // There's only a handful of sounds, so a vector is sufficient.
  struct StreamEntry {
    SoundKey key;
    std::unique_ptr<AudioStreamHandler> handler;
  };
  std::vector<StreamEntry> handlers_;
  std::unique_ptr<service_manager::Connector> connector_;

  DISALLOW_COPY_AND_ASSIGN(SoundsManagerImpl);
};

bool SoundsManagerImpl::Initialize(SoundKey key,
                                   const base::StringPiece& data) {
  if (AudioStreamHandler* handler = GetHandler(key)) {
    DCHECK(handler->IsInitialized());
    return true;
  }

  std::unique_ptr<AudioStreamHandler> handler(
      new AudioStreamHandler(connector_->Clone(), data));
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
  if (!handler)
    return false;
  handler->Stop();
  return true;
}

base::TimeDelta SoundsManagerImpl::GetDuration(SoundKey key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  AudioStreamHandler* handler = GetHandler(key);
  return !handler ? base::TimeDelta() : handler->duration();
}

AudioStreamHandler* SoundsManagerImpl::GetHandler(SoundKey key) {
  for (auto& entry : handlers_) {
    if (entry.key == key)
      return entry.handler.get();
  }
  return nullptr;
}

}  // namespace

SoundsManager::SoundsManager() = default;

SoundsManager::~SoundsManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

// static
void SoundsManager::Create(
    std::unique_ptr<service_manager::Connector> connector) {
  CHECK(!g_instance || g_initialized_for_testing)
      << "SoundsManager::Create() is called twice";
  if (g_initialized_for_testing)
    return;
  g_instance = new SoundsManagerImpl(std::move(connector));
}

// static
void SoundsManager::Shutdown() {
  CHECK(g_instance) << "SoundsManager::Shutdown() is called "
                    << "without previous call to Create()";
  delete g_instance;
  g_instance = NULL;
}

// static
SoundsManager* SoundsManager::Get() {
  CHECK(g_instance) << "SoundsManager::Get() is called before Create()";
  return g_instance;
}

// static
void SoundsManager::InitializeForTesting(SoundsManager* manager) {
  CHECK(!g_instance) << "SoundsManager is already initialized.";
  CHECK(manager);
  g_instance = manager;
  g_initialized_for_testing = true;
}

}  // namespace audio
