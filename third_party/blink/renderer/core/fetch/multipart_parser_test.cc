// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/multipart_parser.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

#include <string.h>
#include <algorithm>

namespace blink {

namespace {

String toString(const Vector<char>& data) {
  if (data.IsEmpty())
    return String("");
  return String(data.data(), data.size());
}

class MockMultipartParserClient final
    : public GarbageCollected<MockMultipartParserClient>,
      public MultipartParser::Client {
  USING_GARBAGE_COLLECTED_MIXIN(MockMultipartParserClient);

 public:
  struct Part {
    Part() = default;
    explicit Part(const HTTPHeaderMap& header_fields)
        : header_fields(header_fields), data_fully_received(false) {}
    HTTPHeaderMap header_fields;
    Vector<char> data;
    bool data_fully_received;
  };
  void PartHeaderFieldsInMultipartReceived(
      const HTTPHeaderMap& header_fields) override {
    parts_.push_back(header_fields);
  }
  void PartDataInMultipartReceived(const char* bytes, size_t size) override {
    parts_.back().data.Append(bytes, SafeCast<wtf_size_t>(size));
  }
  void PartDataInMultipartFullyReceived() override {
    parts_.back().data_fully_received = true;
  }
  const Part& GetPart(wtf_size_t part_index) const {
    EXPECT_LT(part_index, NumberOfParts());
    return part_index < NumberOfParts() ? parts_[part_index] : empty_part_;
  }
  wtf_size_t NumberOfParts() const { return parts_.size(); }

 private:
  Part empty_part_;
  Vector<Part> parts_;
};

constexpr char kBytes[] =
    "preamble"
    "\r\n--boundary\r\n\r\n"
    "\r\n--boundary\r\ncontent-type: application/xhtml+xml\r\n\r\n1"
    "\r\n--boundary\t\r\ncontent-type: "
    "text/html\r\n\r\n2\r\n--\r\n--bound--\r\n--\r\n2\r\n"
    "\r\n--boundary \r\ncontent-type: text/plain; charset=iso-8859-1\r\n\r\n333"
    "\r\n--boundary--\t \r\n"
    "epilogue";

TEST(MultipartParserTest, AppendDataInChunks) {
  const size_t sizes[] = {1u, 2u, strlen(kBytes)};

  Vector<char> boundary;
  boundary.Append("boundary", 8u);
  for (const size_t size : sizes) {
    MockMultipartParserClient* client =
        MakeGarbageCollected<MockMultipartParserClient>();
    MultipartParser* parser =
        MakeGarbageCollected<MultipartParser>(boundary, client);

    for (size_t i = 0u, length = strlen(kBytes); i < length; i += size)
      EXPECT_TRUE(parser->AppendData(kBytes + i, std::min(size, length - i)));
    EXPECT_TRUE(parser->Finish()) << " size=" << size;
    EXPECT_EQ(4u, client->NumberOfParts()) << " size=" << size;
    EXPECT_EQ(0u, client->GetPart(0).header_fields.size());
    EXPECT_EQ(0u, client->GetPart(0).data.size());
    EXPECT_TRUE(client->GetPart(0).data_fully_received);
    EXPECT_EQ(1u, client->GetPart(1).header_fields.size());
    EXPECT_EQ("application/xhtml+xml",
              client->GetPart(1).header_fields.Get(http_names::kContentType));
    EXPECT_EQ("1", toString(client->GetPart(1).data));
    EXPECT_TRUE(client->GetPart(1).data_fully_received);
    EXPECT_EQ(1u, client->GetPart(2).header_fields.size());
    EXPECT_EQ("text/html",
              client->GetPart(2).header_fields.Get(http_names::kContentType));
    EXPECT_EQ("2\r\n--\r\n--bound--\r\n--\r\n2\r\n",
              toString(client->GetPart(2).data));
    EXPECT_TRUE(client->GetPart(2).data_fully_received);
    EXPECT_EQ(1u, client->GetPart(3).header_fields.size());
    EXPECT_EQ("text/plain; charset=iso-8859-1",
              client->GetPart(3).header_fields.Get(http_names::kContentType));
    EXPECT_EQ("333", toString(client->GetPart(3).data));
    EXPECT_TRUE(client->GetPart(3).data_fully_received);
  }
}

TEST(MultipartParserTest, Epilogue) {
  constexpr size_t ends[] = {
      0u,   // Non-empty epilogue in the end.
      8u,   // Empty epilogue in the end.
      9u,   // Partial CRLF after close delimiter in the end.
      10u,  // No CRLF after close delimiter in the end.
      12u,  // No transport padding nor CRLF after close delimiter in the end.
      13u,  // Partial close delimiter in the end.
      14u,  // No close delimiter but a delimiter in the end.
      15u   // Partial delimiter in the end.
  };

  Vector<char> boundary;
  boundary.Append("boundary", 8u);
  for (size_t end : ends) {
    MockMultipartParserClient* client =
        MakeGarbageCollected<MockMultipartParserClient>();
    MultipartParser* parser =
        MakeGarbageCollected<MultipartParser>(boundary, client);

    EXPECT_TRUE(parser->AppendData(kBytes, strlen(kBytes) - end));
    EXPECT_EQ(end <= 12u, parser->Finish()) << " end=" << end;
    EXPECT_EQ(4u, client->NumberOfParts()) << " end=" << end;
    EXPECT_EQ(0u, client->GetPart(0).header_fields.size());
    EXPECT_EQ(0u, client->GetPart(0).data.size());
    EXPECT_TRUE(client->GetPart(0).data_fully_received);
    EXPECT_EQ(1u, client->GetPart(1).header_fields.size());
    EXPECT_EQ("application/xhtml+xml",
              client->GetPart(1).header_fields.Get(http_names::kContentType));
    EXPECT_EQ("1", toString(client->GetPart(1).data));
    EXPECT_TRUE(client->GetPart(1).data_fully_received);
    EXPECT_EQ(1u, client->GetPart(2).header_fields.size());
    EXPECT_EQ("text/html",
              client->GetPart(2).header_fields.Get(http_names::kContentType));
    EXPECT_EQ("2\r\n--\r\n--bound--\r\n--\r\n2\r\n",
              toString(client->GetPart(2).data));
    EXPECT_TRUE(client->GetPart(2).data_fully_received);
    EXPECT_EQ(1u, client->GetPart(3).header_fields.size());
    EXPECT_EQ("text/plain; charset=iso-8859-1",
              client->GetPart(3).header_fields.Get(http_names::kContentType));
    switch (end) {
      case 15u:
        EXPECT_EQ("333\r\n--boundar", toString(client->GetPart(3).data));
        EXPECT_FALSE(client->GetPart(3).data_fully_received);
        break;
      default:
        EXPECT_EQ("333", toString(client->GetPart(3).data));
        EXPECT_TRUE(client->GetPart(3).data_fully_received);
        break;
    }
  }
}

TEST(MultipartParserTest, NoEndBoundary) {
  constexpr char bytes[] =
      "--boundary\r\ncontent-type: application/xhtml+xml\r\n\r\n1";

  Vector<char> boundary;
  boundary.Append("boundary", 8u);
  MockMultipartParserClient* client =
      MakeGarbageCollected<MockMultipartParserClient>();
  MultipartParser* parser =
      MakeGarbageCollected<MultipartParser>(boundary, client);

  EXPECT_TRUE(parser->AppendData(bytes, strlen(bytes)));
  EXPECT_FALSE(parser->Finish());  // No close delimiter.
  EXPECT_EQ(1u, client->NumberOfParts());
  EXPECT_EQ(1u, client->GetPart(0).header_fields.size());
  EXPECT_EQ("application/xhtml+xml",
            client->GetPart(0).header_fields.Get(http_names::kContentType));
  EXPECT_EQ("1", toString(client->GetPart(0).data));
  EXPECT_FALSE(client->GetPart(0).data_fully_received);
}

TEST(MultipartParserTest, NoStartBoundary) {
  constexpr char bytes[] =
      "content-type: application/xhtml+xml\r\n\r\n1\r\n--boundary--\r\n";

  Vector<char> boundary;
  boundary.Append("boundary", 8u);
  MockMultipartParserClient* client =
      MakeGarbageCollected<MockMultipartParserClient>();
  MultipartParser* parser =
      MakeGarbageCollected<MultipartParser>(boundary, client);

  EXPECT_FALSE(parser->AppendData(
      bytes, strlen(bytes)));  // Close delimiter before delimiter.
  EXPECT_EQ(0u, client->NumberOfParts());
}

TEST(MultipartParserTest, NoStartNorEndBoundary) {
  constexpr char bytes[] = "content-type: application/xhtml+xml\r\n\r\n1";

  Vector<char> boundary;
  boundary.Append("boundary", 8u);
  MockMultipartParserClient* client =
      MakeGarbageCollected<MockMultipartParserClient>();
  MultipartParser* parser =
      MakeGarbageCollected<MultipartParser>(boundary, client);

  EXPECT_TRUE(parser->AppendData(bytes, strlen(bytes)));  // Valid preamble.
  EXPECT_FALSE(parser->Finish());                         // No parts.
  EXPECT_EQ(0u, client->NumberOfParts());
}

constexpr size_t kStarts[] = {
    0u,   // Non-empty preamble in the beginning.
    8u,   // Empty preamble in the beginning.
    9u,   // Truncated delimiter in the beginning.
    10u,  // No preamble in the beginning.
    11u   // Truncated dash boundary in the beginning.
};

TEST(MultipartParserTest, Preamble) {
  Vector<char> boundary;
  boundary.Append("boundary", 8u);
  for (const size_t start : kStarts) {
    MockMultipartParserClient* client =
        MakeGarbageCollected<MockMultipartParserClient>();
    MultipartParser* parser =
        MakeGarbageCollected<MultipartParser>(boundary, client);

    EXPECT_TRUE(parser->AppendData(kBytes + start, strlen(kBytes + start)));
    EXPECT_TRUE(parser->Finish());
    switch (start) {
      case 9u:
      case 11u:
        EXPECT_EQ(3u, client->NumberOfParts()) << " start=" << start;
        EXPECT_EQ(1u, client->GetPart(0).header_fields.size());
        EXPECT_EQ("application/xhtml+xml", client->GetPart(0).header_fields.Get(
                                               http_names::kContentType));
        EXPECT_EQ("1", toString(client->GetPart(0).data));
        EXPECT_TRUE(client->GetPart(0).data_fully_received);
        EXPECT_EQ(1u, client->GetPart(1).header_fields.size());
        EXPECT_EQ("text/html", client->GetPart(1).header_fields.Get(
                                   http_names::kContentType));
        EXPECT_EQ("2\r\n--\r\n--bound--\r\n--\r\n2\r\n",
                  toString(client->GetPart(1).data));
        EXPECT_TRUE(client->GetPart(1).data_fully_received);
        EXPECT_EQ(1u, client->GetPart(2).header_fields.size());
        EXPECT_EQ(
            "text/plain; charset=iso-8859-1",
            client->GetPart(2).header_fields.Get(http_names::kContentType));
        EXPECT_EQ("333", toString(client->GetPart(2).data));
        EXPECT_TRUE(client->GetPart(2).data_fully_received);
        break;
      default:
        EXPECT_EQ(4u, client->NumberOfParts()) << " start=" << start;
        EXPECT_EQ(0u, client->GetPart(0).header_fields.size());
        EXPECT_EQ(0u, client->GetPart(0).data.size());
        EXPECT_TRUE(client->GetPart(0).data_fully_received);
        EXPECT_EQ(1u, client->GetPart(1).header_fields.size());
        EXPECT_EQ("application/xhtml+xml", client->GetPart(1).header_fields.Get(
                                               http_names::kContentType));
        EXPECT_EQ("1", toString(client->GetPart(1).data));
        EXPECT_TRUE(client->GetPart(1).data_fully_received);
        EXPECT_EQ(1u, client->GetPart(2).header_fields.size());
        EXPECT_EQ("text/html", client->GetPart(2).header_fields.Get(
                                   http_names::kContentType));
        EXPECT_EQ("2\r\n--\r\n--bound--\r\n--\r\n2\r\n",
                  toString(client->GetPart(2).data));
        EXPECT_TRUE(client->GetPart(2).data_fully_received);
        EXPECT_EQ(1u, client->GetPart(3).header_fields.size());
        EXPECT_EQ(
            "text/plain; charset=iso-8859-1",
            client->GetPart(3).header_fields.Get(http_names::kContentType));
        EXPECT_EQ("333", toString(client->GetPart(3).data));
        EXPECT_TRUE(client->GetPart(3).data_fully_received);
        break;
    }
  }
}

TEST(MultipartParserTest, PreambleWithMalformedBoundary) {
  Vector<char> boundary;
  boundary.Append("--boundary", 10u);
  for (const size_t start : kStarts) {
    MockMultipartParserClient* client =
        MakeGarbageCollected<MockMultipartParserClient>();
    MultipartParser* parser =
        MakeGarbageCollected<MultipartParser>(boundary, client);

    EXPECT_TRUE(parser->AppendData(kBytes + start,
                                   strlen(kBytes + start)));  // Valid preamble.
    EXPECT_FALSE(parser->Finish());                           // No parts.
    EXPECT_EQ(0u, client->NumberOfParts());
  }
}

}  // namespace

}  // namespace blink
