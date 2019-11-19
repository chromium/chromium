// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/resource/multipart_image_resource_parser.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "base/stl_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {
namespace multipart_image_resource_parser_test {

String ToString(const Vector<char>& data) {
  if (data.IsEmpty())
    return String("");
  return String(data.data(), data.size());
}

class MockClient final : public GarbageCollected<MockClient>,
                         public MultipartImageResourceParser::Client {
  USING_GARBAGE_COLLECTED_MIXIN(MockClient);

 public:
  void OnePartInMultipartReceived(const ResourceResponse& response) override {
    responses_.push_back(response);
    data_.push_back(Vector<char>());
  }
  void MultipartDataReceived(const char* bytes, size_t size) override {
    data_.back().Append(bytes, SafeCast<wtf_size_t>(size));
  }

  Vector<ResourceResponse> responses_;
  Vector<Vector<char>> data_;
};

TEST(MultipartResponseTest, SkippableLength) {
  struct {
    const char* input;
    const wtf_size_t position;
    const wtf_size_t expected;
  } line_tests[] = {
      {"Line", 0, 0},         {"Line", 2, 0},         {"Line", 10, 0},
      {"\r\nLine", 0, 2},     {"\nLine", 0, 1},       {"\n\nLine", 0, 1},
      {"\rLine", 0, 0},       {"Line\r\nLine", 4, 2}, {"Line\nLine", 4, 1},
      {"Line\n\nLine", 4, 1}, {"Line\rLine", 4, 0},   {"Line\r\rLine", 4, 0},
  };
  for (size_t i = 0; i < base::size(line_tests); ++i) {
    Vector<char> input;
    input.Append(line_tests[i].input,
                 static_cast<wtf_size_t>(strlen(line_tests[i].input)));
    EXPECT_EQ(line_tests[i].expected,
              MultipartImageResourceParser::SkippableLengthForTest(
                  input, line_tests[i].position));
  }
}

TEST(MultipartResponseTest, FindBoundary) {
  struct {
    const char* boundary;
    const char* data;
    const size_t position;
  } boundary_tests[] = {
      {"bound", "bound", 0},       {"bound", "--bound", 0},
      {"bound", "junkbound", 4},   {"bound", "junk--bound", 4},
      {"foo", "bound", kNotFound}, {"bound", "--boundbound", 0},
  };

  for (size_t i = 0; i < base::size(boundary_tests); ++i) {
    Vector<char> boundary, data;
    boundary.Append(boundary_tests[i].boundary,
                    static_cast<uint32_t>(strlen(boundary_tests[i].boundary)));
    data.Append(boundary_tests[i].data,
                static_cast<uint32_t>(strlen(boundary_tests[i].data)));
    EXPECT_EQ(
        boundary_tests[i].position,
        MultipartImageResourceParser::FindBoundaryForTest(data, &boundary));
  }
}

TEST(MultipartResponseTest, NoStartBoundary) {
  ResourceResponse response(NullURL());
  response.SetMimeType("multipart/x-mixed-replace");
  response.SetHttpHeaderField("Foo", "Bar");
  response.SetHttpHeaderField("Content-type", "text/plain");
  MockClient* client = MakeGarbageCollected<MockClient>();
  Vector<char> boundary;
  boundary.Append("bound", 5);

  MultipartImageResourceParser* parser =
      MakeGarbageCollected<MultipartImageResourceParser>(response, boundary,
                                                         client);
  const char kData[] =
      "Content-type: text/plain\n\n"
      "This is a sample response\n"
      "--bound--"
      "ignore junk after end token --bound\n\nTest2\n";
  parser->AppendData(kData, strlen(kData));
  ASSERT_EQ(1u, client->responses_.size());
  ASSERT_EQ(1u, client->data_.size());
  EXPECT_EQ("This is a sample response", ToString(client->data_[0]));

  parser->Finish();
  ASSERT_EQ(1u, client->responses_.size());
  ASSERT_EQ(1u, client->data_.size());
  EXPECT_EQ("This is a sample response", ToString(client->data_[0]));
}

TEST(MultipartResponseTest, NoEndBoundary) {
  ResourceResponse response(NullURL());
  response.SetMimeType("multipart/x-mixed-replace");
  response.SetHttpHeaderField("Foo", "Bar");
  response.SetHttpHeaderField("Content-type", "text/plain");
  MockClient* client = MakeGarbageCollected<MockClient>();
  Vector<char> boundary;
  boundary.Append("bound", 5);

  MultipartImageResourceParser* parser =
      MakeGarbageCollected<MultipartImageResourceParser>(response, boundary,
                                                         client);
  const char kData[] =
      "bound\nContent-type: text/plain\n\n"
      "This is a sample response\n";
  parser->AppendData(kData, strlen(kData));
  ASSERT_EQ(1u, client->responses_.size());
  ASSERT_EQ(1u, client->data_.size());
  EXPECT_EQ("This is a sample ", ToString(client->data_[0]));

  parser->Finish();
  ASSERT_EQ(1u, client->responses_.size());
  ASSERT_EQ(1u, client->data_.size());
  EXPECT_EQ("This is a sample response\n", ToString(client->data_[0]));
}

TEST(MultipartResponseTest, NoStartAndEndBoundary) {
  ResourceResponse response(NullURL());
  response.SetMimeType("multipart/x-mixed-replace");
  response.SetHttpHeaderField("Foo", "Bar");
  response.SetHttpHeaderField("Content-type", "text/plain");
  MockClient* client = MakeGarbageCollected<MockClient>();
  Vector<char> boundary;
  boundary.Append("bound", 5);

  MultipartImageResourceParser* parser =
      MakeGarbageCollected<MultipartImageResourceParser>(response, boundary,
                                                         client);
  const char kData[] =
      "Content-type: text/plain\n\n"
      "This is a sample response\n";
  parser->AppendData(kData, strlen(kData));
  ASSERT_EQ(1u, client->responses_.size());
  ASSERT_EQ(1u, client->data_.size());
  EXPECT_EQ("This is a sample ", ToString(client->data_[0]));

  parser->Finish();
  ASSERT_EQ(1u, client->responses_.size());
  ASSERT_EQ(1u, client->data_.size());
  EXPECT_EQ("This is a sample response\n", ToString(client->data_[0]));
}

TEST(MultipartResponseTest, MalformedBoundary) {
  // Some servers send a boundary that is prefixed by "--".  See bug 5786.
  ResourceResponse response(NullURL());
  response.SetMimeType("multipart/x-mixed-replace");
  response.SetHttpHeaderField("Foo", "Bar");
  response.SetHttpHeaderField("Content-type", "text/plain");
  MockClient* client = MakeGarbageCollected<MockClient>();
  Vector<char> boundary;
  boundary.Append("--bound", 7);

  MultipartImageResourceParser* parser =
      MakeGarbageCollected<MultipartImageResourceParser>(response, boundary,
                                                         client);
  const char kData[] =
      "--bound\n"
      "Content-type: text/plain\n\n"
      "This is a sample response\n"
      "--bound--"
      "ignore junk after end token --bound\n\nTest2\n";
  parser->AppendData(kData, strlen(kData));
  ASSERT_EQ(1u, client->responses_.size());
  ASSERT_EQ(1u, client->data_.size());
  EXPECT_EQ("This is a sample response", ToString(client->data_[0]));

  parser->Finish();
  ASSERT_EQ(1u, client->responses_.size());
  ASSERT_EQ(1u, client->data_.size());
  EXPECT_EQ("This is a sample response", ToString(client->data_[0]));
}

// Used in for tests that break the data in various places.
struct TestChunk {
  const int start_position;  // offset in data
  const int end_position;    // end offset in data
  const size_t expected_responses;
  const char* expected_data;
};

void VariousChunkSizesTest(const TestChunk chunks[],
                           int chunks_size,
                           size_t responses,
                           int received_data,
                           const char* completed_data) {
  const char kData[] =
      "--bound\n"                    // 0-7
      "Content-type: image/png\n\n"  // 8-32
      "datadatadatadatadata"         // 33-52
      "--bound\n"                    // 53-60
      "Content-type: image/jpg\n\n"  // 61-85
      "foofoofoofoofoo"              // 86-100
      "--bound--";                   // 101-109

  ResourceResponse response(NullURL());
  response.SetMimeType("multipart/x-mixed-replace");
  MockClient* client = MakeGarbageCollected<MockClient>();
  Vector<char> boundary;
  boundary.Append("bound", 5);

  MultipartImageResourceParser* parser =
      MakeGarbageCollected<MultipartImageResourceParser>(response, boundary,
                                                         client);

  for (int i = 0; i < chunks_size; ++i) {
    ASSERT_LT(chunks[i].start_position, chunks[i].end_position);
    parser->AppendData(kData + chunks[i].start_position,
                       chunks[i].end_position - chunks[i].start_position);
    EXPECT_EQ(chunks[i].expected_responses, client->responses_.size());
    EXPECT_EQ(
        String(chunks[i].expected_data),
        client->data_.size() > 0 ? ToString(client->data_.back()) : String(""));
  }
  // Check final state
  parser->Finish();
  EXPECT_EQ(responses, client->responses_.size());
  EXPECT_EQ(completed_data, ToString(client->data_.back()));
}

template <size_t N>
void VariousChunkSizesTest(const TestChunk (&chunks)[N],
                           size_t responses,
                           int received_data,
                           const char* completed_data) {
  VariousChunkSizesTest(chunks, N, responses, received_data, completed_data);
}

TEST(MultipartResponseTest, BreakInBoundary) {
  // Break in the first boundary
  const TestChunk kBound1[] = {
      {0, 4, 0, ""}, {4, 110, 2, "foofoofoofoofoo"},
  };
  VariousChunkSizesTest(kBound1, 2, 2, "foofoofoofoofoo");

  // Break in first and second
  const TestChunk kBound2[] = {
      {0, 4, 0, ""},
      {4, 55, 1, "datadatadatad"},
      {55, 65, 1, "datadatadatadatadata"},
      {65, 110, 2, "foofoofoofoofoo"},
  };
  VariousChunkSizesTest(kBound2, 2, 3, "foofoofoofoofoo");

  // Break in second only
  const TestChunk kBound3[] = {
      {0, 55, 1, "datadatadatad"}, {55, 110, 2, "foofoofoofoofoo"},
  };
  VariousChunkSizesTest(kBound3, 2, 3, "foofoofoofoofoo");
}

TEST(MultipartResponseTest, BreakInHeaders) {
  // Break in first header
  const TestChunk kHeader1[] = {
      {0, 10, 0, ""}, {10, 35, 1, ""}, {35, 110, 2, "foofoofoofoofoo"},
  };
  VariousChunkSizesTest(kHeader1, 2, 2, "foofoofoofoofoo");

  // Break in both headers
  const TestChunk kHeader2[] = {
      {0, 10, 0, ""},
      {10, 65, 1, "datadatadatadatadata"},
      {65, 110, 2, "foofoofoofoofoo"},
  };
  VariousChunkSizesTest(kHeader2, 2, 2, "foofoofoofoofoo");

  // Break at end of a header
  const TestChunk kHeader3[] = {
      {0, 33, 1, ""},
      {33, 65, 1, "datadatadatadatadata"},
      {65, 110, 2, "foofoofoofoofoo"},
  };
  VariousChunkSizesTest(kHeader3, 2, 2, "foofoofoofoofoo");
}

TEST(MultipartResponseTest, BreakInData) {
  // All data as one chunk
  const TestChunk kData1[] = {
      {0, 110, 2, "foofoofoofoofoo"},
  };
  VariousChunkSizesTest(kData1, 2, 2, "foofoofoofoofoo");

  // breaks in data segment
  const TestChunk kData2[] = {
      {0, 35, 1, ""},
      {35, 65, 1, "datadatadatadatadata"},
      {65, 90, 2, ""},
      {90, 110, 2, "foofoofoofoofoo"},
  };
  VariousChunkSizesTest(kData2, 2, 2, "foofoofoofoofoo");

  // Incomplete send
  const TestChunk kData3[] = {
      {0, 35, 1, ""}, {35, 90, 2, ""},
  };
  VariousChunkSizesTest(kData3, 2, 2, "foof");
}

TEST(MultipartResponseTest, SmallChunk) {
  ResourceResponse response(NullURL());
  response.SetMimeType("multipart/x-mixed-replace");
  response.SetHttpHeaderField("Content-type", "text/plain");
  MockClient* client = MakeGarbageCollected<MockClient>();
  Vector<char> boundary;
  boundary.Append("bound", 5);

  MultipartImageResourceParser* parser =
      MakeGarbageCollected<MultipartImageResourceParser>(response, boundary,
                                                         client);

  // Test chunks of size 1, 2, and 0.
  const char kData[] =
      "--boundContent-type: text/plain\n\n"
      "\n--boundContent-type: text/plain\n\n"
      "\n\n--boundContent-type: text/plain\n\n"
      "--boundContent-type: text/plain\n\n"
      "end--bound--";
  parser->AppendData(kData, strlen(kData));
  ASSERT_EQ(4u, client->responses_.size());
  ASSERT_EQ(4u, client->data_.size());
  EXPECT_EQ("", ToString(client->data_[0]));
  EXPECT_EQ("\n", ToString(client->data_[1]));
  EXPECT_EQ("", ToString(client->data_[2]));
  EXPECT_EQ("end", ToString(client->data_[3]));

  parser->Finish();
  ASSERT_EQ(4u, client->responses_.size());
  ASSERT_EQ(4u, client->data_.size());
  EXPECT_EQ("", ToString(client->data_[0]));
  EXPECT_EQ("\n", ToString(client->data_[1]));
  EXPECT_EQ("", ToString(client->data_[2]));
  EXPECT_EQ("end", ToString(client->data_[3]));
}

TEST(MultipartResponseTest, MultipleBoundaries) {
  // Test multiple boundaries back to back
  ResourceResponse response(NullURL());
  response.SetMimeType("multipart/x-mixed-replace");
  MockClient* client = MakeGarbageCollected<MockClient>();
  Vector<char> boundary;
  boundary.Append("bound", 5);

  MultipartImageResourceParser* parser =
      MakeGarbageCollected<MultipartImageResourceParser>(response, boundary,
                                                         client);

  const char kData[] = "--bound\r\n\r\n--bound\r\n\r\nfoofoo--bound--";
  parser->AppendData(kData, strlen(kData));
  ASSERT_EQ(2u, client->responses_.size());
  ASSERT_EQ(2u, client->data_.size());
  EXPECT_EQ("", ToString(client->data_[0]));
  EXPECT_EQ("foofoo", ToString(client->data_[1]));
}

TEST(MultipartResponseTest, EatLeadingLF) {
  ResourceResponse response(NullURL());
  response.SetMimeType("multipart/x-mixed-replace");
  MockClient* client = MakeGarbageCollected<MockClient>();
  Vector<char> boundary;
  boundary.Append("bound", 5);

  const char kData[] =
      "\n\n\n--bound\n\n\ncontent-type: 1\n\n"
      "\n\n\n--bound\n\ncontent-type: 2\n\n"
      "\n\n\n--bound\ncontent-type: 3\n\n";
  MultipartImageResourceParser* parser =
      MakeGarbageCollected<MultipartImageResourceParser>(response, boundary,
                                                         client);

  for (size_t i = 0; i < strlen(kData); ++i)
    parser->AppendData(&kData[i], 1);
  parser->Finish();

  ASSERT_EQ(4u, client->responses_.size());
  ASSERT_EQ(4u, client->data_.size());
  EXPECT_EQ(String(), client->responses_[0].HttpHeaderField("content-type"));
  EXPECT_EQ("", ToString(client->data_[0]));
  EXPECT_EQ(String(), client->responses_[1].HttpHeaderField("content-type"));
  EXPECT_EQ("\ncontent-type: 1\n\n\n\n", ToString(client->data_[1]));
  EXPECT_EQ(String(), client->responses_[2].HttpHeaderField("content-type"));
  EXPECT_EQ("content-type: 2\n\n\n\n", ToString(client->data_[2]));
  EXPECT_EQ("3", client->responses_[3].HttpHeaderField("content-type"));
  EXPECT_EQ("", ToString(client->data_[3]));
}

TEST(MultipartResponseTest, EatLeadingCRLF) {
  ResourceResponse response(NullURL());
  response.SetMimeType("multipart/x-mixed-replace");
  MockClient* client = MakeGarbageCollected<MockClient>();
  Vector<char> boundary;
  boundary.Append("bound", 5);

  const char kData[] =
      "\r\n\r\n\r\n--bound\r\n\r\n\r\ncontent-type: 1\r\n\r\n"
      "\r\n\r\n\r\n--bound\r\n\r\ncontent-type: 2\r\n\r\n"
      "\r\n\r\n\r\n--bound\r\ncontent-type: 3\r\n\r\n";
  MultipartImageResourceParser* parser =
      MakeGarbageCollected<MultipartImageResourceParser>(response, boundary,
                                                         client);

  for (size_t i = 0; i < strlen(kData); ++i)
    parser->AppendData(&kData[i], 1);
  parser->Finish();

  ASSERT_EQ(4u, client->responses_.size());
  ASSERT_EQ(4u, client->data_.size());
  EXPECT_EQ(String(), client->responses_[0].HttpHeaderField("content-type"));
  EXPECT_EQ("", ToString(client->data_[0]));
  EXPECT_EQ(String(), client->responses_[1].HttpHeaderField("content-type"));
  EXPECT_EQ("\r\ncontent-type: 1\r\n\r\n\r\n\r\n", ToString(client->data_[1]));
  EXPECT_EQ(String(), client->responses_[2].HttpHeaderField("content-type"));
  EXPECT_EQ("content-type: 2\r\n\r\n\r\n\r\n", ToString(client->data_[2]));
  EXPECT_EQ("3", client->responses_[3].HttpHeaderField("content-type"));
  EXPECT_EQ("", ToString(client->data_[3]));
}

}  // namespace multipart_image_resource_parser_test
}  // namespace blink
