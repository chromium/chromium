// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/functional/bind.h"
#include "media/base/media_util.h"
#include "media/base/stream_parser_buffer.h"
#include "media/formats/mp2t/es_parser_mpeg1audio.h"

static void NewAudioConfig(const media::AudioDecoderConfig& config) {}
static void EmitBuffer(scoped_refptr<media::StreamParserBuffer> buffer) {}

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  media::NullMediaLog media_log;
  media::mp2t::EsParserMpeg1Audio es_parser(
      base::BindRepeating(&NewAudioConfig), base::BindRepeating(&EmitBuffer),
      &media_log);
  if (es_parser.Parse(data, size, media::kNoTimestamp,
                      media::kNoDecodeTimestamp)) {
    es_parser.Flush();
  }
  return 0;
}
