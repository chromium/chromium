// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the ResponseAnalyzerTests (which test the response
// analyzer's behavior in several parameterized test scenarios) and at the end
// includes the CrossOriginReadBlockingTests, which are more typical unittests.

#include "services/network/orb/orb_sniffers.h"

#include <initializer_list>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "net/base/mime_sniffer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network::orb {

TEST(OrbSnifferTest, SniffForHTML) {
  // Something that technically matches the start of a valid HTML tag.
  EXPECT_EQ(SniffingResult::kYes,
            SniffForHTML("  \t\r\n    <HtMladfokadfkado"));

  // HTML comment followed by whitespace and valid HTML tags.
  EXPECT_EQ(SniffingResult::kYes,
            SniffForHTML(" <!-- this is comment -->\n<html><body>"));

  // HTML comment, whitespace, more HTML comments, HTML tags.
  EXPECT_EQ(
      SniffingResult::kYes,
      SniffForHTML(
          "<!-- this is comment -->\n<!-- this is comment -->\n<html><body>"));

  // HTML comment followed by valid HTML tag.
  EXPECT_EQ(SniffingResult::kYes,
            SniffForHTML("<!-- this is comment <!-- -->\n<script></script>"));

  // Whitespace followed by valid Javascript.
  EXPECT_EQ(SniffingResult::kNo,
            SniffForHTML("        var name=window.location;\nadfadf"));

  // HTML comment followed by valid Javascript.
  EXPECT_EQ(
      SniffingResult::kNo,
      SniffForHTML(
          " <!-- this is comment\n document.write(1);\n// -->\nwindow.open()"));

  // HTML/Javascript polyglot should return kNo.
  EXPECT_EQ(SniffingResult::kNo,
            SniffForHTML(
                "<!--/*--><html><body><script type='text/javascript'><!--//*/\n"
                "var blah = 123;\n"
                "//--></script></body></html>"));

  // Tests to cover more of MaybeSkipHtmlComment.
  EXPECT_EQ(SniffingResult::kMaybe, SniffForHTML("<!-- -/* --><html>"));
  EXPECT_EQ(SniffingResult::kMaybe, SniffForHTML("<!-- --/* --><html>"));
  EXPECT_EQ(SniffingResult::kYes, SniffForHTML("<!-- -/* -->\n<html>"));
  EXPECT_EQ(SniffingResult::kYes, SniffForHTML("<!-- --/* -->\n<html>"));
  EXPECT_EQ(SniffingResult::kMaybe, SniffForHTML("<!----> <html>"));
  EXPECT_EQ(SniffingResult::kYes, SniffForHTML("<!---->\n<html>"));
  EXPECT_EQ(SniffingResult::kYes, SniffForHTML("<!---->\r<html>"));
  EXPECT_EQ(SniffingResult::kYes, SniffForHTML("<!-- ---/-->\n<html><body>"));

  // HTML spec only allows *ASCII* whitespace before the first html element.
  // See also https://html.spec.whatwg.org/multipage/syntax.html and
  // https://infra.spec.whatwg.org/#ascii-whitespace.
  EXPECT_EQ(SniffingResult::kNo, SniffForHTML("<!---->\u2028<html>"));
  EXPECT_EQ(SniffingResult::kNo, SniffForHTML("<!---->\u2029<html>"));

  // Order of line terminators.
  EXPECT_EQ(SniffingResult::kYes, SniffForHTML("<!-- -->\n<b>\rx"));
  EXPECT_EQ(SniffingResult::kYes, SniffForHTML("<!-- -->\r<b>\nx"));
  EXPECT_EQ(SniffingResult::kNo, SniffForHTML("<!-- -->\nx\r<b>"));
  EXPECT_EQ(SniffingResult::kNo, SniffForHTML("<!-- -->\rx\n<b>"));
  EXPECT_EQ(SniffingResult::kYes, SniffForHTML("<!-- -->\n<b>\u2028x"));
  EXPECT_EQ(SniffingResult::kNo, SniffForHTML("<!-- -->\u2028<b>\n<b>"));

  // In UTF8 encoding <LS> is 0xE2 0x80 0xA8 and <PS> is 0xE2 0x80 0xA9.
  // Let's verify that presence of 0xE2 alone doesn't throw
  // FindFirstJavascriptLineTerminator into an infinite loop.
  EXPECT_EQ(SniffingResult::kYes, SniffForHTML("<!-- --> \xe2 \n<b"));
  EXPECT_EQ(SniffingResult::kYes, SniffForHTML("<!-- --> \xe2\x80 \n<b"));
  EXPECT_EQ(SniffingResult::kYes, SniffForHTML("<!-- --> \x80 \n<b"));

  // Commented out html tag followed by non-html (" x").
  std::string_view commented_out_html_tag_data(
      "<!-- <html> <?xml> \n<html>-->\nx");
  EXPECT_EQ(SniffingResult::kNo, SniffForHTML(commented_out_html_tag_data));

  // Prefixes of |commented_out_html_tag_data| should be indeterminate.
  // This covers testing "<!-" as well as "<!-- not terminated yet...".
  std::string_view almost_html = commented_out_html_tag_data;
  while (!almost_html.empty()) {
    almost_html.remove_suffix(1);
    EXPECT_EQ(SniffingResult::kMaybe, SniffForHTML(almost_html)) << almost_html;
  }

  // Explicit tests for an unfinished comment (some also covered by the prefix
  // tests above).
  EXPECT_EQ(SniffingResult::kMaybe, SniffForHTML(""));
  EXPECT_EQ(SniffingResult::kMaybe, SniffForHTML("<!"));
  EXPECT_EQ(SniffingResult::kMaybe, SniffForHTML("<!-- unterminated..."));
  EXPECT_EQ(SniffingResult::kMaybe,
            SniffForHTML("<!-- blah --> <html> no newline yet"));
}

TEST(OrbSnifferTest, SniffForXML) {
  std::string_view xml_data(
      "   \t \r \n     <?xml version=\"1.0\"?>\n <catalog");
  std::string_view non_xml_data("        var name=window.location;\nadfadf");
  std::string_view empty_data("");

  EXPECT_EQ(SniffingResult::kYes, SniffForXML(xml_data));
  EXPECT_EQ(SniffingResult::kNo, SniffForXML(non_xml_data));

  // Empty string should be indeterminate.
  EXPECT_EQ(SniffingResult::kMaybe, SniffForXML(empty_data));
}

TEST(OrbSnifferTest, SniffForJSON) {
  std::string_view json_data("\t\t\r\n   { \"name\" : \"chrome\", ");
  std::string_view json_corrupt_after_first_key(
      "\t\t\r\n   { \"name\" :^^^^!!@#\1\", ");
  std::string_view json_data2("{ \"key   \\\"  \"          \t\t\r\n:");
  std::string_view non_json_data0("\t\t\r\n   { name : \"chrome\", ");
  std::string_view non_json_data1("\t\t\r\n   foo({ \"name\" : \"chrome\", ");

  EXPECT_EQ(SniffingResult::kYes, SniffForJSON(json_data));
  EXPECT_EQ(SniffingResult::kYes, SniffForJSON(json_corrupt_after_first_key));

  EXPECT_EQ(SniffingResult::kYes, SniffForJSON(json_data2));

  // All prefixes prefixes of |json_data2| ought to be indeterminate.
  std::string_view almost_json = json_data2;
  while (!almost_json.empty()) {
    almost_json.remove_suffix(1);
    EXPECT_EQ(SniffingResult::kMaybe, SniffForJSON(almost_json)) << almost_json;
  }

  EXPECT_EQ(SniffingResult::kNo, SniffForJSON(non_json_data0));
  EXPECT_EQ(SniffingResult::kNo, SniffForJSON(non_json_data1));

  EXPECT_EQ(SniffingResult::kYes, SniffForJSON(R"({"" : 1})"))
      << "Empty strings are accepted";
  EXPECT_EQ(SniffingResult::kNo, SniffForJSON(R"({'' : 1})"))
      << "Single quotes are not accepted";
  EXPECT_EQ(SniffingResult::kYes, SniffForJSON("{\"\\\"\" : 1}"))
      << "Escaped quotes are recognized";
  EXPECT_EQ(SniffingResult::kYes, SniffForJSON(R"({"\\\u000a" : 1})"))
      << "Escaped control characters are recognized";
  EXPECT_EQ(SniffingResult::kMaybe, SniffForJSON(R"({"\\\u00)"))
      << "Incomplete escape results in maybe";
  EXPECT_EQ(SniffingResult::kMaybe, SniffForJSON("{\"\\"))
      << "Incomplete escape results in maybe";
  EXPECT_EQ(SniffingResult::kMaybe, SniffForJSON("{\"\\\""))
      << "Incomplete escape results in maybe";
  EXPECT_EQ(SniffingResult::kYes, SniffForJSON("{\"\n\" : true}"))
      << "Unescaped control characters are accepted (a bit more like "
      << "Javascript than strict reading of the JSON spec)";
  EXPECT_EQ(SniffingResult::kNo, SniffForJSON("{}"))
      << "Empty dictionary is not recognized (since it's valid JS too)";
  EXPECT_EQ(SniffingResult::kNo, SniffForJSON("[true, false, 1, 2]"))
      << "Lists dictionary are not recognized (since they're valid JS too)";
  EXPECT_EQ(SniffingResult::kNo, SniffForJSON(R"({":"})"))
      << "A colon character inside a string does not trigger a match";
}

}  // namespace network::orb
