// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <random>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media.h"
#include "media/base/media_util.h"
#include "media/filters/vpx_video_decoder.h"

struct Env {
  Env() {
    media::InitializeMediaLibrary();
    base::CommandLine::Init(0, nullptr);
    logging::SetMinLogLevel(logging::LOGGING_FATAL);
  }

  base::AtExitManager at_exit_manager;
  base::test::SingleThreadTaskEnvironment task_environment;
};

void OnDecodeComplete(base::OnceClosure quit_closure,
                      media::DecoderStatus status) {
  std::move(quit_closure).Run();
}

void OnInitDone(base::OnceClosure quit_closure,
                bool* success_dest,
                media::DecoderStatus status) {
  *success_dest = status.is_ok();
  std::move(quit_closure).Run();
}

void OnOutputComplete(scoped_refptr<media::VideoFrame> frame) {}

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data_ptr, size_t size) {
  // SAFETY: LibFuzzer must pass a valid `data_ptr` and `size`.
  auto data = UNSAFE_BUFFERS(base::span(data_ptr, size));

  // Create Env on the first run of LLVMFuzzerTestOneInput otherwise
  // message_loop will be created before this process forks when used with AFL,
  // causing hangs.
  [[maybe_unused]] static Env* env = new Env();
  std::mt19937_64 rng;

  {  // Seed rng from data.
    std::string str(base::as_string_view(data));
    std::size_t data_hash = std::hash<std::string>()(str);
    rng.seed(data_hash);
  }

  // Compute randomized constants. Put all rng() usages here.
  // Use only values that pass DCHECK in VpxVideoDecoder::ConfigureDecoder().
  media::VideoCodec codec;

  bool has_alpha = false;
  if (rng() & 1) {
    codec = media::VideoCodec::kVP8;
    // non-Alpha VP8 decoding isn't supported by VpxVideoDecoder on Linux.
    has_alpha = true;
  } else {
    codec = media::VideoCodec::kVP9;
    has_alpha = rng() & 1;
  }

  auto profile = static_cast<media::VideoCodecProfile>(
      rng() % media::VIDEO_CODEC_PROFILE_MAX);
  auto color_space =
      media::VideoColorSpace(rng() % 256, rng() % 256, rng() % 256,
                             (rng() & 1) ? gfx::ColorSpace::RangeID::LIMITED
                                         : gfx::ColorSpace::RangeID::FULL);
  auto rotation =
      static_cast<media::VideoRotation>(rng() % media::VIDEO_ROTATION_MAX);
  auto coded_size = gfx::Size(1 + (rng() % 127), 1 + (rng() % 127));
  auto visible_rect = gfx::Rect(coded_size);
  auto natural_size = gfx::Size(1 + (rng() % 127), 1 + (rng() % 127));
  uint8_t reflection = rng() % 4;

  media::VideoDecoderConfig config(
      codec, profile,
      has_alpha ? media::VideoDecoderConfig::AlphaMode::kHasAlpha
                : media::VideoDecoderConfig::AlphaMode::kIsOpaque,
      color_space, media::VideoTransformation(rotation, reflection), coded_size,
      visible_rect, natural_size, media::EmptyExtraData(),
      media::EncryptionScheme::kUnencrypted);

  if (!config.IsValidConfig())
    return 0;

  media::VpxVideoDecoder decoder;

  {
    base::RunLoop run_loop;
    bool success = false;
    decoder.Initialize(
        config, true /* low_delay */, nullptr /* cdm_context */,
        base::BindOnce(&OnInitDone, run_loop.QuitClosure(), &success),
        base::BindRepeating(&OnOutputComplete), base::NullCallback());
    run_loop.Run();
    if (!success)
      return 0;
  }

  {
    base::RunLoop run_loop;
    auto buffer = media::DecoderBuffer::CopyFrom(data);
    decoder.Decode(buffer,
                   base::BindOnce(&OnDecodeComplete, run_loop.QuitClosure()));
    run_loop.Run();
  }

  return 0;
}
