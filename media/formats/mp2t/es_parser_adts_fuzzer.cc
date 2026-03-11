// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/mp2t/es_parser_adts.h"

#include <stddef.h>
#include <stdint.h>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "media/base/stream_parser_buffer.h"
#include "testing/libfuzzer/libfuzzer_base_wrappers.h"

static void NewAudioConfig(const media::AudioDecoderConfig& config) {}
static void EmitBuffer(scoped_refptr<media::StreamParserBuffer> buffer) {}

// Entry point for LibFuzzer.
DEFINE_LLVM_FUZZER_TEST_ONE_INPUT_SPAN(const base::span<const uint8_t> data) {
  media::mp2t::EsParserAdts es_parser(base::BindRepeating(&NewAudioConfig),
                                      base::BindRepeating(&EmitBuffer), true);
  if (!es_parser.Parse(data, media::kNoTimestamp, media::kNoDecodeTimestamp)) {
    return 0;
  }
  es_parser.Flush();
  return 0;
}
