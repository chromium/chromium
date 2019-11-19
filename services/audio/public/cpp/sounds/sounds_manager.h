// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_PUBLIC_CPP_SOUNDS_SOUNDS_MANAGER_H_
#define SERVICES_AUDIO_PUBLIC_CPP_SOUNDS_SOUNDS_MANAGER_H_

#include <memory>

#include "base/macros.h"
#include "base/sequence_checker.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "media/base/media_export.h"
#include "services/service_manager/public/cpp/connector.h"

namespace audio {

// This class is used for reproduction of system sounds. All methods
// should be accessed from the Audio thread.
class SoundsManager {
 public:
  typedef int SoundKey;

  // Creates a singleton instance of the SoundsManager.
  static void Create(std::unique_ptr<service_manager::Connector> connector);

  // Removes a singleton instance of the SoundsManager.
  static void Shutdown();

  // Returns a pointer to a singleton instance of the SoundsManager.
  static SoundsManager* Get();

  // Initializes sounds manager for testing. The |manager| will be owned
  // by the internal pointer and will be deleted by Shutdown().
  static void InitializeForTesting(SoundsManager* manager);

  // Initializes SoundsManager with the wav data for the system
  // sounds. Returns true if SoundsManager was successfully
  // initialized.
  virtual bool Initialize(SoundKey key, const base::StringPiece& data) = 0;

  // Plays sound identified by |key|, returns false if SoundsManager
  // was not properly initialized.
  virtual bool Play(SoundKey key) = 0;

  // Stops playing sound identified by |key|, returns false if SoundsManager
  // was not properly initialized.
  virtual bool Stop(SoundKey key) = 0;

  // Returns duration of the sound identified by |key|. If SoundsManager
  // was not properly initialized or |key| was not registered, this
  // method returns an empty value.
  virtual base::TimeDelta GetDuration(SoundKey key) = 0;

 protected:
  SoundsManager();
  virtual ~SoundsManager();

  SEQUENCE_CHECKER(sequence_checker_);

 private:
  DISALLOW_COPY_AND_ASSIGN(SoundsManager);
};

}  // namespace audio

#endif  // SERVICES_AUDIO_PUBLIC_CPP_SOUNDS_SOUNDS_MANAGER_H_
