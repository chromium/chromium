// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <cstdint>
#include <ctime>
#include <vector>

#include "base/rand_util.h"
#include "net/third_party/spdy/core/hpack/hpack_constants.h"
#include "net/third_party/spdy/core/hpack/hpack_decoder_adapter.h"
#include "net/third_party/spdy/core/hpack/hpack_encoder.h"
#include "net/third_party/spdy/core/spdy_test_utils.h"
#include "net/third_party/spdy/platform/api/spdy_string.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace spdy {
namespace test {

namespace {

// Supports testing with the input split at every byte boundary.
enum InputSizeParam { ALL_INPUT, ONE_BYTE, ZERO_THEN_ONE_BYTE };

class HpackRoundTripTest : public ::testing::TestWithParam<InputSizeParam> {
 protected:
  HpackRoundTripTest() : encoder_(ObtainHpackHuffmanTable()), decoder_() {}

  void SetUp() override {
    // Use a small table size to tickle eviction handling.
    encoder_.ApplyHeaderTableSizeSetting(256);
    decoder_.ApplyHeaderTableSizeSetting(256);
  }

  bool RoundTrip(const SpdyHeaderBlock& header_set) {
    SpdyString encoded;
    encoder_.EncodeHeaderSet(header_set, &encoded);

    bool success = true;
    if (GetParam() == ALL_INPUT) {
      // Pass all the input to the decoder at once.
      success = decoder_.HandleControlFrameHeadersData(encoded.data(),
                                                       encoded.size());
    } else if (GetParam() == ONE_BYTE) {
      // Pass the input to the decoder one byte at a time.
      const char* data = encoded.data();
      for (size_t ndx = 0; ndx < encoded.size() && success; ++ndx) {
        success = decoder_.HandleControlFrameHeadersData(data + ndx, 1);
      }
    } else if (GetParam() == ZERO_THEN_ONE_BYTE) {
      // Pass the input to the decoder one byte at a time, but before each
      // byte pass an empty buffer.
      const char* data = encoded.data();
      for (size_t ndx = 0; ndx < encoded.size() && success; ++ndx) {
        success = (decoder_.HandleControlFrameHeadersData(data + ndx, 0) &&
                   decoder_.HandleControlFrameHeadersData(data + ndx, 1));
      }
    } else {
      ADD_FAILURE() << "Unknown param: " << GetParam();
    }

    if (success) {
      success = decoder_.HandleControlFrameHeadersComplete(nullptr);
    }

    EXPECT_EQ(header_set, decoder_.decoded_block());
    return success;
  }

  size_t SampleExponential(size_t mean, size_t sanity_bound) {
    return std::min<size_t>(-std::log(base::RandDouble()) * mean, sanity_bound);
  }

  HpackEncoder encoder_;
  HpackDecoderAdapter decoder_;
};

INSTANTIATE_TEST_CASE_P(Tests,
                        HpackRoundTripTest,
                        ::testing::Values(ALL_INPUT,
                                          ONE_BYTE,
                                          ZERO_THEN_ONE_BYTE));

TEST_P(HpackRoundTripTest, ResponseFixtures) {
  {
    SpdyHeaderBlock headers;
    headers[":status"] = "302";
    headers["cache-control"] = "private";
    headers["date"] = "Mon, 21 Oct 2013 20:13:21 GMT";
    headers["location"] = "https://www.example.com";
    EXPECT_TRUE(RoundTrip(headers));
  }
  {
    SpdyHeaderBlock headers;
    headers[":status"] = "200";
    headers["cache-control"] = "private";
    headers["date"] = "Mon, 21 Oct 2013 20:13:21 GMT";
    headers["location"] = "https://www.example.com";
    EXPECT_TRUE(RoundTrip(headers));
  }
  {
    SpdyHeaderBlock headers;
    headers[":status"] = "200";
    headers["cache-control"] = "private";
    headers["content-encoding"] = "gzip";
    headers["date"] = "Mon, 21 Oct 2013 20:13:22 GMT";
    headers["location"] = "https://www.example.com";
    headers["set-cookie"] =
        "foo=ASDJKHQKBZXOQWEOPIUAXQWEOIU;"
        " max-age=3600; version=1";
    headers["multivalue"] = SpdyString("foo\0bar", 7);
    EXPECT_TRUE(RoundTrip(headers));
  }
}

TEST_P(HpackRoundTripTest, RequestFixtures) {
  {
    SpdyHeaderBlock headers;
    headers[":authority"] = "www.example.com";
    headers[":method"] = "GET";
    headers[":path"] = "/";
    headers[":scheme"] = "http";
    headers["cookie"] = "baz=bing; foo=bar";
    EXPECT_TRUE(RoundTrip(headers));
  }
  {
    SpdyHeaderBlock headers;
    headers[":authority"] = "www.example.com";
    headers[":method"] = "GET";
    headers[":path"] = "/";
    headers[":scheme"] = "http";
    headers["cache-control"] = "no-cache";
    headers["cookie"] = "foo=bar; spam=eggs";
    EXPECT_TRUE(RoundTrip(headers));
  }
  {
    SpdyHeaderBlock headers;
    headers[":authority"] = "www.example.com";
    headers[":method"] = "GET";
    headers[":path"] = "/index.html";
    headers[":scheme"] = "https";
    headers["custom-key"] = "custom-value";
    headers["cookie"] = "baz=bing; fizzle=fazzle; garbage";
    headers["multivalue"] = SpdyString("foo\0bar", 7);
    EXPECT_TRUE(RoundTrip(headers));
  }
}

TEST_P(HpackRoundTripTest, RandomizedExamples) {
  // Grow vectors of names & values, which are seeded with fixtures and then
  // expanded with dynamically generated data. Samples are taken using the
  // exponential distribution.
  std::vector<SpdyString> pseudo_header_names, random_header_names;
  pseudo_header_names.push_back(":authority");
  pseudo_header_names.push_back(":path");
  pseudo_header_names.push_back(":status");

  // TODO(jgraettinger): Enable "cookie" as a name fixture. Crumbs may be
  // reconstructed in any order, which breaks the simple validation used here.

  std::vector<SpdyString> values;
  values.push_back("/");
  values.push_back("/index.html");
  values.push_back("200");
  values.push_back("404");
  values.push_back("");
  values.push_back("baz=bing; foo=bar; garbage");
  values.push_back("baz=bing; fizzle=fazzle; garbage");

  int seed = std::time(NULL);
  LOG(INFO) << "Seeding with srand(" << seed << ")";
  srand(seed);

  for (size_t i = 0; i != 2000; ++i) {
    SpdyHeaderBlock headers;

    // Choose a random number of headers to add, and of these a random subset
    // will be HTTP/2 pseudo headers.
    size_t header_count = 1 + SampleExponential(7, 50);
    size_t pseudo_header_count =
        std::min(header_count, 1 + SampleExponential(7, 50));
    EXPECT_LE(pseudo_header_count, header_count);
    for (size_t j = 0; j != header_count; ++j) {
      SpdyString name, value;
      // Pseudo headers must be added before regular headers.
      if (j < pseudo_header_count) {
        // Choose one of the defined pseudo headers at random.
        size_t name_index = base::RandGenerator(pseudo_header_names.size());
        name = pseudo_header_names[name_index];
      } else {
        // Randomly reuse an existing header name, or generate a new one.
        size_t name_index = SampleExponential(20, 200);
        if (name_index >= random_header_names.size()) {
          name = base::RandBytesAsString(1 + SampleExponential(5, 30));
          // A regular header cannot begin with the pseudo header prefix ":".
          if (name[0] == ':') {
            name[0] = 'x';
          }
          random_header_names.push_back(name);
        } else {
          name = random_header_names[name_index];
        }
      }

      // Randomly reuse an existing value, or generate a new one.
      size_t value_index = SampleExponential(20, 200);
      if (value_index >= values.size()) {
        SpdyString newvalue =
            base::RandBytesAsString(1 + SampleExponential(15, 75));
        // Currently order is not preserved in the encoder.  In particular,
        // when a value is decomposed at \0 delimiters, its parts might get
        // encoded out of order if some but not all of them already exist in
        // the header table.  For now, avoid \0 bytes in values.
        std::replace(newvalue.begin(), newvalue.end(), '\x00', '\x01');
        values.push_back(newvalue);
        value = values.back();
      } else {
        value = values[value_index];
      }
      headers[name] = value;
    }
    EXPECT_TRUE(RoundTrip(headers));
  }
}

}  // namespace

}  // namespace test
}  // namespace spdy
