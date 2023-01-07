// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_FAKE_AUDIO_LOG_FACTORY_H_
#define MEDIA_AUDIO_FAKE_AUDIO_LOG_FACTORY_H_

#include <memory>

#include "base/compiler_specific.h"
#include "media/audio/audio_logging.h"
#include "media/base/media_export.h"

namespace media {

// Creates stub AudioLog instances, for testing, which do nothing.
class MEDIA_EXPORT FakeAudioLogFactory : public AudioLogFactory {
 public:
  FakeAudioLogFactory();

  FakeAudioLogFactory(const FakeAudioLogFactory&) = delete;
  FakeAudioLogFactory& operator=(const FakeAudioLogFactory&) = delete;

  ~FakeAudioLogFactory() override;
  std::unique_ptr<AudioLog> CreateAudioLog(AudioComponent component,
                                           int component_id) override;
};

}  // namespace media

#endif  // MEDIA_AUDIO_FAKE_AUDIO_LOG_FACTORY_H_
