// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/audio_decoder.h"

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "content/public/test/blink_test_environment.h"
#include "media/base/media.h"
#include "third_party/blink/public/platform/web_audio_bus.h"

struct Environment {
  Environment() {
    base::CommandLine::Init(0, nullptr);
    blink_environment_.SetUp();

    // Suppress WARNING messages from the debug build.
    logging::SetMinLogLevel(logging::LOGGING_FATAL);

    // This is needed to suppress noisy log messages from ffmpeg.
    media::InitializeMediaLibrary();
  }

  content::BlinkTestEnvironment blink_environment_;
  base::AtExitManager at_exit;
};

Environment* env = new Environment();

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // Larger inputs are likely to trigger timeouts and OOMs which would not be
  // considered as valid bugs.
  if (size > 8 * 1024)
    return 0;

  blink::WebAudioBus web_audio_bus;
  bool success = content::DecodeAudioFileData(
      &web_audio_bus, reinterpret_cast<const char*>(data), size);

  if (!success)
    return 0;

  return 0;
}
