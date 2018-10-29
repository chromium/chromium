/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <map>

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/date_components.h"
#include "third_party/blink/renderer/platform/mhtml/mhtml_archive.h"
#include "third_party/blink/renderer/platform/mhtml/mhtml_parser.h"
#include "third_party/blink/renderer/platform/serialized_resource.h"
#include "third_party/blink/renderer/platform/shared_buffer.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/wtf/time.h"

using blink::url_test_helpers::ToKURL;

namespace blink {
namespace test {

namespace {

const char kEndOfPartBoundary[] = "--boundary-example";
const char kEndOfDocumentBoundary[] = "--boundary-example--";

}  // namespace

class MHTMLArchiveTest : public testing::Test {
 public:
  MHTMLArchiveTest() {
    file_path_ = test::CoreTestDataPath("frameserializer/css/");
    mhtml_date_ = WTF::Time::FromJsTime(1520551829000);
    mhtml_date_header_ = String::FromUTF8("Thu, 8 Mar 2018 23:30:29 -0000");
  }

 protected:
  void AddResource(const char* url,
                   const char* mime,
                   scoped_refptr<SharedBuffer> data) {
    SerializedResource resource(ToKURL(url), mime, std::move(data));
    resources_.push_back(resource);
  }

  void AddResource(const char* url, const char* mime, const char* file_name) {
    AddResource(url, mime, ReadFile(file_name));
  }

  void AddTestMainResource() {
    AddResource("http://www.test.com", "text/html", "css_test_page.html");
  }

  void AddTestResources() {
    AddResource("http://www.test.com", "text/html", "css_test_page.html");
    AddResource("http://www.test.com/link_styles.css", "text/css",
                "link_styles.css");
    AddResource("http://www.test.com/import_style_from_link.css", "text/css",
                "import_style_from_link.css");
    AddResource("http://www.test.com/import_styles.css", "text/css",
                "import_styles.css");
    AddResource("http://www.test.com/red_background.png", "image/png",
                "red_background.png");
    AddResource("http://www.test.com/orange_background.png", "image/png",
                "orange_background.png");
    AddResource("http://www.test.com/yellow_background.png", "image/png",
                "yellow_background.png");
    AddResource("http://www.test.com/green_background.png", "image/png",
                "green_background.png");
    AddResource("http://www.test.com/blue_background.png", "image/png",
                "blue_background.png");
    AddResource("http://www.test.com/purple_background.png", "image/png",
                "purple_background.png");
    AddResource("http://www.test.com/ul-dot.png", "image/png", "ul-dot.png");
    AddResource("http://www.test.com/ol-dot.png", "image/png", "ol-dot.png");
  }

  std::map<std::string, std::string> ExtractHeaders(LineReader& line_reader) {
    // Read the data per line until reaching the empty line.
    std::map<std::string, std::string> mhtml_headers;
    std::string line;
    line_reader.GetNextLine(&line);
    while (line.length()) {
      // Peek next line to see if it starts with soft line break. If yes, append
      // to current line.
      std::string next_line;
      while (true) {
        line_reader.GetNextLine(&next_line);
        if (next_line.length() > 1 &&
            (next_line[0] == ' ' || next_line[0] == '\t')) {
          line += &(next_line.at(1));
          continue;
        }
        break;
      }

      std::string::size_type pos = line.find(':');
      if (pos == std::string::npos)
        continue;
      std::string key = line.substr(0, pos);
      std::string value = line.substr(pos + 2);
      mhtml_headers.emplace(key, value);

      line = next_line;
    }
    return mhtml_headers;
  }

  std::map<std::string, std::string> ExtractMHTMLHeaders() {
    LineReader line_reader(std::string(mhtml_data_.data(), mhtml_data_.size()));
    return ExtractHeaders(line_reader);
  }

  void GenerateMHTMLData(const Vector<SerializedResource>& resources,
                         MHTMLArchive::EncodingPolicy encoding_policy,
                         const KURL& url,
                         const String& title,
                         const String& mime_type) {
    // This boundary is as good as any other.  Plus it gets used in almost
    // all the examples in the MHTML spec - RFC 2557.
    String boundary = String::FromUTF8("boundary-example");

    MHTMLArchive::GenerateMHTMLHeader(boundary, url, title, mime_type,
                                      mhtml_date_, mhtml_data_);
    for (const auto& resource : resources) {
      MHTMLArchive::GenerateMHTMLPart(boundary, String(), encoding_policy,
                                      resource, mhtml_data_);
    }
    MHTMLArchive::GenerateMHTMLFooterForTesting(boundary, mhtml_data_);

    // Validate the generated MHTML.
    MHTMLParser parser(
        SharedBuffer::Create(mhtml_data_.data(), mhtml_data_.size()));
    EXPECT_FALSE(parser.ParseArchive().IsEmpty())
        << "Generated MHTML is malformed";
  }

  void Serialize(const KURL& url,
                 const String& title,
                 const String& mime,
                 MHTMLArchive::EncodingPolicy encoding_policy) {
    return GenerateMHTMLData(resources_, encoding_policy, url, title, mime);
  }

  Vector<char>& mhtml_data() { return mhtml_data_; }

  WTF::Time mhtml_date() const { return mhtml_date_; }
  const String& mhtml_date_header() const { return mhtml_date_header_; }

 private:
  scoped_refptr<SharedBuffer> ReadFile(const char* file_name) {
    String file_path = file_path_ + file_name;
    return test::ReadFromFile(file_path);
  }

  String file_path_;
  Vector<SerializedResource> resources_;
  Vector<char> mhtml_data_;
  WTF::Time mhtml_date_;
  String mhtml_date_header_;
};

TEST_F(MHTMLArchiveTest,
       TestMHTMLHeadersWithTitleContainingAllPrintableCharacters) {
  const char kURL[] = "http://www.example.com/";
  const char kTitle[] = "abc";
  AddTestMainResource();
  Serialize(ToKURL(kURL), String::FromUTF8(kTitle), "text/html",
            MHTMLArchive::kUseDefaultEncoding);

  std::map<std::string, std::string> mhtml_headers = ExtractMHTMLHeaders();

  EXPECT_EQ("<Saved by Blink>", mhtml_headers["From"]);
  EXPECT_FALSE(mhtml_headers["Date"].empty());
  EXPECT_EQ(
      "multipart/related;type=\"text/html\";boundary=\"boundary-example\"",
      mhtml_headers["Content-Type"]);
  EXPECT_EQ("abc", mhtml_headers["Subject"]);
  EXPECT_EQ(kURL, mhtml_headers["Snapshot-Content-Location"]);
}

TEST_F(MHTMLArchiveTest,
       TestMHTMLHeadersWithTitleContainingNonPrintableCharacters) {
  const char kURL[] = "http://www.example.com/";
  const char kTitle[] = "abc \t=\xe2\x98\x9d\xf0\x9f\x8f\xbb";
  AddTestMainResource();
  Serialize(ToKURL(kURL), String::FromUTF8(kTitle), "text/html",
            MHTMLArchive::kUseDefaultEncoding);

  std::map<std::string, std::string> mhtml_headers = ExtractMHTMLHeaders();

  EXPECT_EQ("<Saved by Blink>", mhtml_headers["From"]);
  EXPECT_FALSE(mhtml_headers["Date"].empty());
  EXPECT_EQ(
      "multipart/related;type=\"text/html\";boundary=\"boundary-example\"",
      mhtml_headers["Content-Type"]);
  EXPECT_EQ("=?utf-8?Q?abc=20=09=3D=E2=98=9D=F0=9F=8F=BB?=",
            mhtml_headers["Subject"]);
  EXPECT_EQ(kURL, mhtml_headers["Snapshot-Content-Location"]);
}

TEST_F(MHTMLArchiveTest,
       TestMHTMLHeadersWithLongTitleContainingNonPrintableCharacters) {
  const char kURL[] = "http://www.example.com/";
  const char kTitle[] =
      "01234567890123456789012345678901234567890123456789"
      "01234567890123456789012345678901234567890123456789"
      " \t=\xe2\x98\x9d\xf0\x9f\x8f\xbb";
  AddTestMainResource();
  Serialize(ToKURL(kURL), String::FromUTF8(kTitle), "text/html",
            MHTMLArchive::kUseDefaultEncoding);

  std::map<std::string, std::string> mhtml_headers = ExtractMHTMLHeaders();

  EXPECT_EQ("<Saved by Blink>", mhtml_headers["From"]);
  EXPECT_FALSE(mhtml_headers["Date"].empty());
  EXPECT_EQ(
      "multipart/related;type=\"text/html\";boundary=\"boundary-example\"",
      mhtml_headers["Content-Type"]);
  EXPECT_EQ(
      "=?utf-8?Q?012345678901234567890123456789"
      "012345678901234567890123456789012?="
      "=?utf-8?Q?345678901234567890123456789"
      "0123456789=20=09=3D=E2=98=9D=F0=9F?="
      "=?utf-8?Q?=8F=BB?=",
      mhtml_headers["Subject"]);
  EXPECT_EQ(kURL, mhtml_headers["Snapshot-Content-Location"]);
}

TEST_F(MHTMLArchiveTest, TestMHTMLPartsWithBinaryEncoding) {
  const char kURL[] = "http://www.example.com";
  AddTestResources();
  Serialize(ToKURL(kURL), "Test Serialization", "text/html",
            MHTMLArchive::kUseBinaryEncoding);

  // Read the MHTML data line per line and do some pseudo-parsing to make sure
  // the right encoding is used for the different sections.
  LineReader line_reader(std::string(mhtml_data().data(), mhtml_data().size()));
  int part_count = 0;
  std::string line, last_line;
  while (line_reader.GetNextLine(&line)) {
    last_line = line;
    if (line != kEndOfPartBoundary)
      continue;
    part_count++;

    std::map<std::string, std::string> part_headers =
        ExtractHeaders(line_reader);
    EXPECT_FALSE(part_headers["Content-Type"].empty());
    EXPECT_EQ("binary", part_headers["Content-Transfer-Encoding"]);
    EXPECT_FALSE(part_headers["Content-Location"].empty());
  }
  EXPECT_EQ(12, part_count);

  // Last line should be the end-of-document boundary.
  EXPECT_EQ(kEndOfDocumentBoundary, last_line);
}

TEST_F(MHTMLArchiveTest, TestMHTMLPartsWithDefaultEncoding) {
  const char kURL[] = "http://www.example.com";
  AddTestResources();
  Serialize(ToKURL(kURL), "Test Serialization", "text/html",
            MHTMLArchive::kUseDefaultEncoding);

  // Read the MHTML data line per line and do some pseudo-parsing to make sure
  // the right encoding is used for the different sections.
  LineReader line_reader(std::string(mhtml_data().data(), mhtml_data().size()));
  int part_count = 0;
  std::string line, last_line;
  while (line_reader.GetNextLine(&line)) {
    last_line = line;
    if (line != kEndOfPartBoundary)
      continue;
    part_count++;

    std::map<std::string, std::string> part_headers =
        ExtractHeaders(line_reader);

    std::string content_type = part_headers["Content-Type"];
    EXPECT_FALSE(content_type.empty());

    std::string encoding = part_headers["Content-Transfer-Encoding"];
    EXPECT_FALSE(encoding.empty());

    if (content_type.compare(0, 5, "text/") == 0)
      EXPECT_EQ("quoted-printable", encoding);
    else if (content_type.compare(0, 6, "image/") == 0)
      EXPECT_EQ("base64", encoding);
    else
      FAIL() << "Unexpected Content-Type: " << content_type;
  }
  EXPECT_EQ(12, part_count);

  // Last line should be the end-of-document boundary.
  EXPECT_EQ(kEndOfDocumentBoundary, last_line);
}

TEST_F(MHTMLArchiveTest, MHTMLFromScheme) {
  const char kURL[] = "http://www.example.com";
  AddTestMainResource();
  Serialize(ToKURL(kURL), "Test Serialization", "text/html",
            MHTMLArchive::kUseDefaultEncoding);

  scoped_refptr<SharedBuffer> data =
      SharedBuffer::Create(mhtml_data().data(), mhtml_data().size());
  KURL http_url = ToKURL("http://www.example.com");
  KURL content_url = ToKURL("content://foo");
  KURL file_url = ToKURL("file://foo");
  KURL special_scheme_url = ToKURL("fooscheme://bar");

  // MHTMLArchives can only be initialized from local schemes, http/https
  // schemes, and content scheme(Android specific).
  EXPECT_NE(nullptr, MHTMLArchive::Create(http_url, data.get()));
#if defined(OS_ANDROID)
  EXPECT_NE(nullptr, MHTMLArchive::Create(content_url, data.get()));
#else
  EXPECT_EQ(nullptr, MHTMLArchive::Create(content_url, data.get()));
#endif
  EXPECT_NE(nullptr, MHTMLArchive::Create(file_url, data.get()));
  EXPECT_EQ(nullptr, MHTMLArchive::Create(special_scheme_url, data.get()));
  SchemeRegistry::RegisterURLSchemeAsLocal("fooscheme");
  EXPECT_NE(nullptr, MHTMLArchive::Create(special_scheme_url, data.get()));
}

TEST_F(MHTMLArchiveTest, MHTMLDate) {
  const char kURL[] = "http://www.example.com";
  AddTestMainResource();
  Serialize(ToKURL(kURL), "Test Serialization", "text/html",
            MHTMLArchive::kUseDefaultEncoding);
  // The serialization process should have added a date header corresponding to
  // mhtml_date().
  std::map<std::string, std::string> mhtml_headers = ExtractMHTMLHeaders();
  ASSERT_EQ(mhtml_date_header(),
            String::FromUTF8(mhtml_headers["Date"].c_str()));

  scoped_refptr<SharedBuffer> data =
      SharedBuffer::Create(mhtml_data().data(), mhtml_data().size());
  KURL http_url = ToKURL("http://www.example.com");
  MHTMLArchive* archive = MHTMLArchive::Create(http_url, data.get());
  ASSERT_NE(nullptr, archive);

  // The deserialization process should have parsed the header into a Date.
  EXPECT_EQ(mhtml_date(), archive->Date());
}

TEST_F(MHTMLArchiveTest, EmptyArchive) {
  char* buf = nullptr;
  scoped_refptr<SharedBuffer> data =
      SharedBuffer::Create(buf, static_cast<size_t>(0u));
  KURL http_url = ToKURL("http://www.example.com");
  MHTMLArchive* archive = MHTMLArchive::Create(http_url, data.get());
  EXPECT_EQ(nullptr, archive);
}

TEST_F(MHTMLArchiveTest, NoMainResource) {
  const char kURL[] = "http://www.example.com";
  // Only add a resource to a CSS file, so no main resource is valid for
  // rendering.
  AddResource("http://www.example.com/link_styles.css", "text/css",
              "link_styles.css");
  Serialize(ToKURL(kURL), "Test Serialization", "text/html",
            MHTMLArchive::kUseDefaultEncoding);

  scoped_refptr<SharedBuffer> data =
      SharedBuffer::Create(mhtml_data().data(), mhtml_data().size());
  KURL http_url = ToKURL("http://www.example.com");
  MHTMLArchive* archive = MHTMLArchive::Create(http_url, data.get());

  EXPECT_EQ(nullptr, archive);
}

}  // namespace test

}  // namespace blink
