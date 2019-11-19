// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/fuzzing/hpack_fuzz_util.h"

#include <algorithm>
#include <cmath>
#include <memory>

#include "base/rand_util.h"
#include "base/sys_byteorder.h"
#include "net/third_party/quiche/src/spdy/core/hpack/hpack_constants.h"
#include "net/third_party/quiche/src/spdy/platform/api/spdy_ptr_util.h"

namespace spdy {

namespace {

// Sampled exponential distribution parameters:
// Number of headers in each header set.
const size_t kHeaderCountMean = 7;
const size_t kHeaderCountMax = 50;
// Selected index within list of headers.
const size_t kHeaderIndexMean = 20;
const size_t kHeaderIndexMax = 200;
// Approximate distribution of header name lengths.
const size_t kNameLengthMean = 5;
const size_t kNameLengthMax = 30;
// Approximate distribution of header value lengths.
const size_t kValueLengthMean = 15;
const size_t kValueLengthMax = 75;

}  //  namespace

using base::RandBytesAsString;
using std::map;

HpackFuzzUtil::GeneratorContext::GeneratorContext() = default;
HpackFuzzUtil::GeneratorContext::~GeneratorContext() = default;

HpackFuzzUtil::Input::Input() : offset(0) {}
HpackFuzzUtil::Input::~Input() = default;

HpackFuzzUtil::FuzzerContext::FuzzerContext() = default;
HpackFuzzUtil::FuzzerContext::~FuzzerContext() = default;

// static
void HpackFuzzUtil::InitializeGeneratorContext(GeneratorContext* context) {
  // Seed the generator with common header fixtures.
  context->names.push_back(":authority");
  context->names.push_back(":path");
  context->names.push_back(":status");
  context->names.push_back("cookie");
  context->names.push_back("content-type");
  context->names.push_back("cache-control");
  context->names.push_back("date");
  context->names.push_back("user-agent");
  context->names.push_back("via");

  context->values.push_back("/");
  context->values.push_back("/index.html");
  context->values.push_back("200");
  context->values.push_back("404");
  context->values.push_back("");
  context->values.push_back("baz=bing; foo=bar; garbage");
  context->values.push_back("baz=bing; fizzle=fazzle; garbage");
  context->values.push_back("rudolph=the-red-nosed-reindeer");
  context->values.push_back("had=a;very_shiny=nose");
  context->values.push_back("and\0if\0you\0ever\1saw\0it;");
  context->values.push_back("u; would=even;say-it\xffglows");
}

// static
SpdyHeaderBlock HpackFuzzUtil::NextGeneratedHeaderSet(
    GeneratorContext* context) {
  SpdyHeaderBlock headers;

  size_t header_count =
      1 + SampleExponential(kHeaderCountMean, kHeaderCountMax);
  for (size_t j = 0; j != header_count; ++j) {
    size_t name_index = SampleExponential(kHeaderIndexMean, kHeaderIndexMax);
    size_t value_index = SampleExponential(kHeaderIndexMean, kHeaderIndexMax);
    std::string name, value;
    if (name_index >= context->names.size()) {
      context->names.push_back(RandBytesAsString(
          1 + SampleExponential(kNameLengthMean, kNameLengthMax)));
      name = context->names.back();
    } else {
      name = context->names[name_index];
    }
    if (value_index >= context->values.size()) {
      context->values.push_back(RandBytesAsString(
          1 + SampleExponential(kValueLengthMean, kValueLengthMax)));
      value = context->values.back();
    } else {
      value = context->values[value_index];
    }
    headers[name] = value;
  }
  return headers;
}

// static
size_t HpackFuzzUtil::SampleExponential(size_t mean, size_t sanity_bound) {
  return std::min(static_cast<size_t>(-std::log(base::RandDouble()) * mean),
                  sanity_bound);
}

// static
bool HpackFuzzUtil::NextHeaderBlock(Input* input, SpdyStringPiece* out) {
  // ClusterFuzz may truncate input files if the fuzzer ran out of allocated
  // disk space. Be tolerant of these.
  CHECK_LE(input->offset, input->input.size());
  if (input->remaining() < sizeof(uint32_t)) {
    return false;
  }

  size_t length =
      base::NetToHost32(*reinterpret_cast<const uint32_t*>(input->ptr()));
  input->offset += sizeof(uint32_t);

  if (input->remaining() < length) {
    return false;
  }
  *out = SpdyStringPiece(input->ptr(), length);
  input->offset += length;
  return true;
}

// static
std::string HpackFuzzUtil::HeaderBlockPrefix(size_t block_size) {
  uint32_t length = base::HostToNet32(static_cast<uint32_t>(block_size));
  return std::string(reinterpret_cast<char*>(&length), sizeof(uint32_t));
}

// static
void HpackFuzzUtil::InitializeFuzzerContext(FuzzerContext* context) {
  context->first_stage = std::make_unique<HpackDecoderAdapter>();
  context->second_stage =
      std::make_unique<HpackEncoder>(ObtainHpackHuffmanTable());
  context->third_stage = std::make_unique<HpackDecoderAdapter>();
}

// static
bool HpackFuzzUtil::RunHeaderBlockThroughFuzzerStages(
    FuzzerContext* context,
    SpdyStringPiece input_block) {
  // First stage: Decode the input header block. This may fail on invalid input.
  if (!context->first_stage->HandleControlFrameHeadersData(
          input_block.data(), input_block.size())) {
    return false;
  }
  if (!context->first_stage->HandleControlFrameHeadersComplete(nullptr)) {
    return false;
  }
  // Second stage: Re-encode the decoded header block. This must succeed.
  std::string second_stage_out;
  CHECK(context->second_stage->EncodeHeaderSet(
      context->first_stage->decoded_block(), &second_stage_out));

  // Third stage: Expect a decoding of the re-encoded block to succeed, but
  // don't require it. It's possible for the stage-two encoder to produce an
  // output which violates decoder size tolerances.
  if (!context->third_stage->HandleControlFrameHeadersData(
          second_stage_out.data(), second_stage_out.length())) {
    return false;
  }
  if (!context->third_stage->HandleControlFrameHeadersComplete(nullptr)) {
    return false;
  }
  return true;
}

// static
void HpackFuzzUtil::FlipBits(uint8_t* buffer,
                             size_t buffer_length,
                             size_t flip_per_thousand) {
  uint64_t buffer_bit_length = buffer_length * 8u;
  uint64_t bits_to_flip = flip_per_thousand * (1 + buffer_bit_length / 1024);

  // Iteratively identify & flip offsets in the buffer bit-sequence.
  for (uint64_t i = 0; i != bits_to_flip; ++i) {
    uint64_t bit_offset = base::RandUint64() % buffer_bit_length;
    buffer[bit_offset / 8u] ^= (1 << (bit_offset % 8u));
  }
}

}  // namespace spdy
