// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "extensions/browser/api/web_request/form_data_parser.h"

#include <stddef.h>

#include <string_view>

#include "base/check.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

// Attempts to create a parser corresponding to the |content_type_header|.
// On success, returns the parser.
std::unique_ptr<FormDataParser> InitParser(
    const std::string& content_type_header) {
  std::unique_ptr<FormDataParser> parser(
      FormDataParser::CreateFromContentTypeHeader(&content_type_header));
  if (parser.get() == nullptr) {
    return nullptr;
  }
  return parser;
}

// Attempts to run the parser corresponding to the |content_type_header|
// on the source represented by the concatenation of blocks from |bytes|.
// On success, returns true and the parsed |output|, else false.
// Parsed |output| has names on even positions (0, 2, ...), values on odd ones.
bool RunParser(const std::string& content_type_header,
               const std::vector<const std::string_view*>& bytes,
               std::vector<std::string>* output) {
  DCHECK(output);
  output->clear();
  std::unique_ptr<FormDataParser> parser(InitParser(content_type_header));
  if (!parser.get()) {
    return false;
  }
  FormDataParser::Result result;
  for (size_t block = 0; block < bytes.size(); ++block) {
    if (!parser->SetSource(*(bytes[block]))) {
      return false;
    }
    while (parser->GetNextNameValue(&result)) {
      output->push_back(result.name());
      base::Value value = result.take_value();
      if (value.is_string()) {
        output->push_back(value.GetString());
      } else {
        const auto& blob = value.GetBlob();
        output->push_back(std::string(blob.begin(), blob.end()));
      }
    }
  }
  return parser->AllDataReadOK();
}

// Attempts to run the parser corresponding to the |content_type_header|
// on the source represented by the concatenation of blocks from |bytes|.
// Checks that the parser fails parsing.
bool CheckParserFails(const std::string& content_type_header,
                      const std::vector<const std::string_view*>& bytes) {
  std::vector<std::string> output;
  std::unique_ptr<FormDataParser> parser(InitParser(content_type_header));
  if (!parser.get()) {
    return false;
  }
  FormDataParser::Result result;
  for (size_t block = 0; block < bytes.size(); ++block) {
    if (!parser->SetSource(*(bytes[block]))) {
      break;
    }
    while (parser->GetNextNameValue(&result)) {
      output.push_back(result.name());
      base::Value value = result.take_value();
      if (value.is_string()) {
        output.push_back(value.GetString());
      } else {
        const auto& blob = value.GetBlob();
        output.push_back(std::string(blob.begin(), blob.end()));
      }
    }
  }
  return !parser->AllDataReadOK();
}

}  // namespace

TEST(WebRequestFormDataParserTest, Parsing) {
  // We verify that POST data parsers cope with various formats of POST data.
  // Construct the test data.
  const std::string kBoundary = "THIS_IS_A_BOUNDARY";
  const std::string kBlockStr1 =
      std::string("--") + kBoundary +
      "\r\n"
      "Content-Disposition: form-data; name=\"text\"\r\n"
      "\r\n"
      "test\rtext\nwith non-CRLF line breaks\r\n"
      "--" +
      kBoundary +
      "\r\n"
      "Content-Disposition: form-data; name=\"file\"; filename=\"test\"\r\n"
      "Content-Type: application/octet-stream\r\n"
      "\r\n";
  const std::string kBlockStr2 =
      std::string("\r\n--") + kBoundary +
      "\r\n"
      "Content-Disposition: form-data; name=\"password\"\r\n"
      "\r\n"
      "test password\r\n"
      "--" +
      kBoundary +
      "\r\n"
      "Content-Disposition: form-data; name=\"radio\"\r\n"
      "\r\n"
      "Yes\r\n"
      "--" +
      kBoundary +
      "\r\n"
      "Content-Disposition: form-data; name=\"check\"\r\n"
      "\r\n"
      "option A\r\n"
      "--" +
      kBoundary +
      "\r\n"
      "Content-Disposition: form-data; name=\"check\"\r\n"
      "\r\n"
      "option B\r\n"
      "--" +
      kBoundary +
      "\r\n"
      "Content-Disposition: form-data; name=\"txtarea\"\r\n"
      "\r\n"
      "Some text.\r\n"
      "Other.\r\n"
      "\r\n"
      "--" +
      kBoundary +
      "\r\n"
      "Content-Disposition: form-data; name=\"select\"\r\n"
      "\r\n"
      "one\r\n"
      "--" +
      kBoundary + "\r\n";
  const std::string kBlockStr3 =
      "Content-Disposition: form-data; name=\"binary\"\r\n"
      "Content-Type: application/octet-stream\r\n"
      "\r\n"
      "\u0420\u043e\u0434\u0436\u0435\u0440 "
      "\u0416\u0435\u043b\u044f\u0437\u043d\u044b"
      "\r\n"
      "--" +
      kBoundary + "--";
  // POST data input.
  const std::string kBigBlock = kBlockStr1 + kBlockStr2 + kBlockStr3;
  const std::string kUrlEncodedBlock =
      "text=test%0Dtext%0Awith+non-CRLF+line+breaks"
      "&file=test&password=test+password&radio=Yes&check=option+A"
      "&check=option+B&txtarea=Some+text.%0D%0AOther.%0D%0A&select=one"
      "&binary=%D0%A0%D0%BE%D0%B4%D0%B6%D0%B5%D1%80%20%D0%96%D0%B5%D0%BB%D1%8F%"
      "D0%B7%D0%BD%D1%8B";
  const std::string_view kMultipartBytes(kBigBlock);
  const std::string_view kMultipartBytesSplit1(kBlockStr1);
  const std::string_view kMultipartBytesSplit2(kBlockStr2);
  const std::string_view kMultipartBytesSplit3(kBlockStr3);
  const std::string_view kUrlEncodedBytes(kUrlEncodedBlock);
  const std::string kPlainBlock = "abc";
  const std::string_view kTextPlainBytes(kPlainBlock);
  // Headers.
  const std::string kUrlEncoded = "application/x-www-form-urlencoded";
  const std::string kTextPlain = "text/plain";
  const std::string kMultipart =
      std::string("multipart/form-data; boundary=") + kBoundary;
  // Expected output.
  const char* kPairs[] = {"text",
                          "test\rtext\nwith non-CRLF line breaks",
                          "file",
                          "test",
                          "password",
                          "test password",
                          "radio",
                          "Yes",
                          "check",
                          "option A",
                          "check",
                          "option B",
                          "txtarea",
                          "Some text.\r\nOther.\r\n",
                          "select",
                          "one",
                          "binary",
                          ("\u0420\u043e\u0434\u0436\u0435\u0440 "
                           "\u0416\u0435\u043b\u044f\u0437\u043d\u044b")};
  const std::vector<std::string> kExpected(kPairs, kPairs + std::size(kPairs));

  std::vector<const std::string_view*> input;
  std::vector<std::string> output;

  // First test: multipart POST data in one lump.
  input.push_back(&kMultipartBytes);
  EXPECT_TRUE(RunParser(kMultipart, input, &output));
  EXPECT_EQ(kExpected, output);

  // Second test: multipart POST data in several lumps.
  input.clear();
  input.push_back(&kMultipartBytesSplit1);
  input.push_back(&kMultipartBytesSplit2);
  input.push_back(&kMultipartBytesSplit3);
  EXPECT_TRUE(RunParser(kMultipart, input, &output));
  EXPECT_EQ(kExpected, output);

  // Third test: URL-encoded POST data.
  input.clear();
  input.push_back(&kUrlEncodedBytes);
  EXPECT_TRUE(RunParser(kUrlEncoded, input, &output));
  EXPECT_EQ(kExpected, output);

  // Fourth test: text/plain POST data in one lump.
  input.clear();
  input.push_back(&kTextPlainBytes);
  // This should fail, text/plain is ambiguous and thus unparseable.
  EXPECT_FALSE(RunParser(kTextPlain, input, &output));
}

TEST(WebRequestFormDataParserTest, MalformedPayload) {
  // We verify that POST data parsers reject malformed data.
  // Construct the test data.
  const std::string kBoundary = "THIS_IS_A_BOUNDARY";
  const std::string kBlockStr =
      std::string("--") + kBoundary +
      "\r\n"
      "Content-Disposition: form-data; name=\"text\"\r\n"
      "\r\n"
      "test\rtext\nwith non-CRLF line breaks\r\n"
      "-" +
      kBoundary +
      "\r\n" /* Missing '-'. */
      "Content-Disposition: form-data; name=\"file\"; filename=\"test\"\r\n"
      "Content-Type: application/octet-stream\r\n"
      /* Two CRLF missing. */
      "--" +
      kBoundary +
      "\r\n"
      "Content-Disposition: form-data; name=\"select\"\r\n"
      "\r\n"
      "one\r\n"
      "--" +
      kBoundary + "-" /* Missing '-' at the end. */;
  // POST data input.
  // The following block is corrupted -- contains a "==" substring.
  const std::string kUrlEncodedBlock =
      "text=test%0Dtext%0Awith+non-CRLF+line+breaks"
      "&file==test&password=test+password&radio=Yes&check=option+A"
      "&check=option+B&txtarea=Some+text.%0D%0AOther.%0D%0A&select=one";
  const std::string_view kMultipartBytes(kBlockStr);
  const std::string_view kMultipartBytesEmpty("");
  const std::string_view kUrlEncodedBytes(kUrlEncodedBlock);
  const std::string_view kUrlEncodedBytesEmpty("");
  // Headers.
  const std::string kUrlEncoded = "application/x-www-form-urlencoded";
  const std::string kMultipart =
      std::string("multipart/form-data; boundary=") + kBoundary;

  std::vector<const std::string_view*> input;

  // First test: malformed multipart POST data.
  input.push_back(&kMultipartBytes);
  EXPECT_TRUE(CheckParserFails(kMultipart, input));

  // Second test: empty multipart POST data.
  input.clear();
  input.push_back(&kMultipartBytesEmpty);
  EXPECT_TRUE(CheckParserFails(kMultipart, input));

  // Third test: malformed URL-encoded POST data.
  input.clear();
  input.push_back(&kUrlEncodedBytes);
  EXPECT_TRUE(CheckParserFails(kUrlEncoded, input));

  // Fourth test: empty URL-encoded POST data. Note that an empty string is a
  // valid url-encoded value, so this should parse correctly.
  std::vector<std::string> output;
  input.clear();
  input.push_back(&kUrlEncodedBytesEmpty);
  EXPECT_TRUE(RunParser(kUrlEncoded, input, &output));
  EXPECT_EQ(0u, output.size());
}

}  // namespace extensions
