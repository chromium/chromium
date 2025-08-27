// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_API_SPEECH_TTS_ENGINE_MANIFEST_HANDLER_H_
#define EXTENSIONS_COMMON_API_SPEECH_TTS_ENGINE_MANIFEST_HANDLER_H_

#include <set>
#include <string>
#include <vector>

#include "base/values.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handler.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

struct TtsVoice {
  TtsVoice();
  TtsVoice(const TtsVoice& other);
  ~TtsVoice();

  std::string voice_name;
  std::string lang;
  std::string gender;
  bool remote;
  std::set<std::string> event_types;
};

struct TtsEngine : public Extension::ManifestData {
  TtsEngine();
  ~TtsEngine() override;
  static bool Parse(const base::Value::List& tts_voices,
                    TtsEngine* out_engine,
                    std::u16string* error);

  std::vector<extensions::TtsVoice> voices;

  // The sample rate at which this engine encodes its audio data.
  std::optional<int> sample_rate;

  // The number of samples in one audio buffer.
  std::optional<int> buffer_size;

  static const std::vector<TtsVoice>* GetTtsVoices(const Extension* extension);
  static const TtsEngine* GetTtsEngineInfo(const Extension* extension);
};

// Parses the "tts_engine" manifest key.
class TtsEngineManifestHandler : public ManifestHandler {
 public:
  TtsEngineManifestHandler();

  TtsEngineManifestHandler(const TtsEngineManifestHandler&) = delete;
  TtsEngineManifestHandler& operator=(const TtsEngineManifestHandler&) = delete;

  ~TtsEngineManifestHandler() override;

  bool Parse(Extension* extension, std::u16string* error) override;

 private:
  base::span<const char* const> Keys() const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_API_SPEECH_TTS_ENGINE_MANIFEST_HANDLER_H_
