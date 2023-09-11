// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_view_source_document.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"

namespace blink {

class HTMLViewSourceDocumentTest : public SimTest {
 public:
  void LoadMainResource(const String& html) {
    SimRequest main_resource("https://example.com/", "text/html");
    LoadURL("https://example.com/");
    main_resource.Complete(html);
    Compositor().BeginFrame();
  }

  void SetUp() override {
    SimTest::SetUp();
    MainFrame().EnableViewSourceMode(true);
  }
};

TEST_F(HTMLViewSourceDocumentTest, ViewSource1) {
  LoadMainResource(R"HTML(
      <!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01//EN"
      "http://www.w3.org/TR/html4/strict.dtd">
      <hr noshade width=75%>
      <div align="center" title="" id="foo">
      <p>hello world</p>
      </div>
  )HTML");
  EXPECT_EQ(
      GetDocument().documentElement()->outerHTML(),
      "<html><head><meta name=\"color-scheme\" content=\"light "
      "dark\"></head><body><div class=\"line-gutter-backdrop\"></div><form "
      "autocomplete=\"off\"><label class=\"line-wrap-control\">"
      "<input type=\"checkbox\"></label>"
      "</form><table><tbody><tr><td class=\"line-number\" value=\"1\"></td><td "
      "class=\"line-content\"><br></td></tr><tr><td class=\"line-number\" "
      "value=\"2\"></td><td class=\"line-content\">      <span "
      "class=\"html-doctype\">&lt;!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML "
      "4.01//EN\"</span></td></tr><tr><td class=\"line-number\" "
      "value=\"3\"></td><td class=\"line-content\"><span "
      "class=\"html-doctype\">      "
      "\"http://www.w3.org/TR/html4/strict.dtd\"&gt;</span></td></tr><tr><td "
      "class=\"line-number\" value=\"4\"></td><td class=\"line-content\">      "
      "<span class=\"html-tag\">&lt;hr <span "
      "class=\"html-attribute-name\">noshade</span> <span "
      "class=\"html-attribute-name\">width</span>=<span "
      "class=\"html-attribute-value\">75%</span>&gt;</span></td></tr><tr><td "
      "class=\"line-number\" value=\"5\"></td><td class=\"line-content\">      "
      "<span class=\"html-tag\">&lt;div <span "
      "class=\"html-attribute-name\">align</span>=\"<span "
      "class=\"html-attribute-value\">center</span>\" <span "
      "class=\"html-attribute-name\">title</span>=\"\" <span "
      "class=\"html-attribute-name\">id</span>=\"<span "
      "class=\"html-attribute-value\">foo</span>\"&gt;</span></td></tr><tr><td "
      "class=\"line-number\" value=\"6\"></td><td class=\"line-content\">      "
      "<span class=\"html-tag\">&lt;p&gt;</span>hello world<span "
      "class=\"html-tag\">&lt;/p&gt;</span></td></tr><tr><td "
      "class=\"line-number\" value=\"7\"></td><td class=\"line-content\">      "
      "<span class=\"html-tag\">&lt;/div&gt;</span></td></tr><tr><td "
      "class=\"line-number\" value=\"8\"></td><td class=\"line-content\">  "
      "<span "
      "class=\"html-end-of-file\"></span></td></tr></tbody></table></body></"
      "html>");
}

TEST_F(HTMLViewSourceDocumentTest, ViewSource2) {
  LoadMainResource(R"HTML(
      <script>
      <testscript>
      </script>

      <style>
      <teststyle>
      </style>

      <xmp>
      <testxmp>
      </xmp>

      <textarea>
      <testtextarea>
      </textarea>
  )HTML");
  EXPECT_EQ(
      GetDocument().documentElement()->outerHTML(),
      "<html><head><meta name=\"color-scheme\" content=\"light "
      "dark\"></head><body><div class=\"line-gutter-backdrop\"></div><form "
      "autocomplete=\"off\"><label class=\"line-wrap-control\">"
      "<input type=\"checkbox\"></label>"
      "</form><table><tbody><tr><td class=\"line-number\" value=\"1\"></td><td "
      "class=\"line-content\"><br></td></tr><tr><td class=\"line-number\" "
      "value=\"2\"></td><td class=\"line-content\">      <span "
      "class=\"html-tag\">&lt;script&gt;</span></td></tr><tr><td "
      "class=\"line-number\" value=\"3\"></td><td class=\"line-content\">      "
      "&lt;testscript&gt;</td></tr><tr><td class=\"line-number\" "
      "value=\"4\"></td><td class=\"line-content\">      <span "
      "class=\"html-tag\">&lt;/script&gt;</span></td></tr><tr><td "
      "class=\"line-number\" value=\"5\"></td><td "
      "class=\"line-content\"><br></td></tr><tr><td class=\"line-number\" "
      "value=\"6\"></td><td class=\"line-content\">      <span "
      "class=\"html-tag\">&lt;style&gt;</span></td></tr><tr><td "
      "class=\"line-number\" value=\"7\"></td><td class=\"line-content\">      "
      "&lt;teststyle&gt;</td></tr><tr><td class=\"line-number\" "
      "value=\"8\"></td><td class=\"line-content\">      <span "
      "class=\"html-tag\">&lt;/style&gt;</span></td></tr><tr><td "
      "class=\"line-number\" value=\"9\"></td><td "
      "class=\"line-content\"><br></td></tr><tr><td class=\"line-number\" "
      "value=\"10\"></td><td class=\"line-content\">      <span "
      "class=\"html-tag\">&lt;xmp&gt;</span></td></tr><tr><td "
      "class=\"line-number\" value=\"11\"></td><td class=\"line-content\">     "
      " &lt;testxmp&gt;</td></tr><tr><td class=\"line-number\" "
      "value=\"12\"></td><td class=\"line-content\">      <span "
      "class=\"html-tag\">&lt;/xmp&gt;</span></td></tr><tr><td "
      "class=\"line-number\" value=\"13\"></td><td "
      "class=\"line-content\"><br></td></tr><tr><td class=\"line-number\" "
      "value=\"14\"></td><td class=\"line-content\">      <span "
      "class=\"html-tag\">&lt;textarea&gt;</span></td></tr><tr><td "
      "class=\"line-number\" value=\"15\"></td><td class=\"line-content\">     "
      " &lt;testtextarea&gt;</td></tr><tr><td class=\"line-number\" "
      "value=\"16\"></td><td class=\"line-content\">      <span "
      "class=\"html-tag\">&lt;/textarea&gt;</span></td></tr><tr><td "
      "class=\"line-number\" value=\"17\"></td><td class=\"line-content\">  "
      "<span "
      "class=\"html-end-of-file\"></span></td></tr></tbody></table></body></"
      "html>");
}

TEST_F(HTMLViewSourceDocumentTest, ViewSource3) {
  LoadMainResource(R"HTML(
      <head><base href="http://example.org/foo/"></head>
      <body>
      <a href="bar">http://example.org/foo/bar</a><br>
      <a href="/bar">http://example.org/bar</a><br>
      <a href="http://example.org/foobar">http://example.org/foobar</a><br>
      <a href="bar?a&amp;b">http://example.org/foo/bar?a&b</a>
      </body>
  )HTML");
  EXPECT_EQ(
      GetDocument().documentElement()->outerHTML(),
      "<html><head><meta name=\"color-scheme\" content=\"light "
      "dark\"></head><body><div class=\"line-gutter-backdrop\"></div><form "
      "autocomplete=\"off\"><label class=\"line-wrap-control\">"
      "<input type=\"checkbox\"></label>"
      "</form><table><tbody><tr><td class=\"line-number\" value=\"1\"></td><td "
      "class=\"line-content\"><br></td></tr><tr><td class=\"line-number\" "
      "value=\"2\"></td><td class=\"line-content\">      <span "
      "class=\"html-tag\">&lt;head&gt;</span><span class=\"html-tag\">&lt;base "
      "<span class=\"html-attribute-name\">href</span><base "
      "href=\"http://example.org/foo/\">=\"<a class=\"html-attribute-value "
      "html-resource-link\" target=\"_blank\" href=\"http://example.org/foo/\" "
      "rel=\"noreferrer "
      "noopener\">http://example.org/foo/</a>\"&gt;</span><span "
      "class=\"html-tag\">&lt;/head&gt;</span></td></tr><tr><td "
      "class=\"line-number\" value=\"3\"></td><td class=\"line-content\">      "
      "<span class=\"html-tag\">&lt;body&gt;</span></td></tr><tr><td "
      "class=\"line-number\" value=\"4\"></td><td class=\"line-content\">      "
      "<span class=\"html-tag\">&lt;a <span "
      "class=\"html-attribute-name\">href</span>=\"<a "
      "class=\"html-attribute-value html-external-link\" target=\"_blank\" "
      "href=\"bar\" rel=\"noreferrer "
      "noopener\">bar</a>\"&gt;</span>http://example.org/foo/bar<span "
      "class=\"html-tag\">&lt;/a&gt;</span><span "
      "class=\"html-tag\">&lt;br&gt;</span></td></tr><tr><td "
      "class=\"line-number\" value=\"5\"></td><td class=\"line-content\">      "
      "<span class=\"html-tag\">&lt;a <span "
      "class=\"html-attribute-name\">href</span>=\"<a "
      "class=\"html-attribute-value html-external-link\" target=\"_blank\" "
      "href=\"/bar\" rel=\"noreferrer "
      "noopener\">/bar</a>\"&gt;</span>http://example.org/bar<span "
      "class=\"html-tag\">&lt;/a&gt;</span><span "
      "class=\"html-tag\">&lt;br&gt;</span></td></tr><tr><td "
      "class=\"line-number\" value=\"6\"></td><td class=\"line-content\">      "
      "<span class=\"html-tag\">&lt;a <span "
      "class=\"html-attribute-name\">href</span>=\"<a "
      "class=\"html-attribute-value html-external-link\" target=\"_blank\" "
      "href=\"http://example.org/foobar\" rel=\"noreferrer "
      "noopener\">http://example.org/foobar</a>\"&gt;</span>http://example.org/"
      "foobar<span class=\"html-tag\">&lt;/a&gt;</span><span "
      "class=\"html-tag\">&lt;br&gt;</span></td></tr><tr><td "
      "class=\"line-number\" value=\"7\"></td><td class=\"line-content\">      "
      "<span class=\"html-tag\">&lt;a <span "
      "class=\"html-attribute-name\">href</span>=\"<a "
      "class=\"html-attribute-value html-external-link\" target=\"_blank\" "
      "href=\"bar?a&amp;b\" rel=\"noreferrer "
      "noopener\">bar?a&amp;amp;b</a>\"&gt;</span>http://example.org/foo/"
      "bar?a&amp;b<span class=\"html-tag\">&lt;/a&gt;</span></td></tr><tr><td "
      "class=\"line-number\" value=\"8\"></td><td class=\"line-content\">      "
      "<span class=\"html-tag\">&lt;/body&gt;</span></td></tr><tr><td "
      "class=\"line-number\" value=\"9\"></td><td class=\"line-content\">  "
      "<span "
      "class=\"html-end-of-file\"></span></td></tr></tbody></table></body></"
      "html>");
}

TEST_F(HTMLViewSourceDocumentTest, ViewSource4) {
  LoadMainResource(R"HTML(
      <HEAD><BASE HREF="http://example.org/foo/"></HEAD>
      <BODY>
      <A HREF="bar">http://example.org/foo/bar</A><BR>
      <A HREF="/bar">http://example.org/bar</A><BR>
      <A HREF="http://example.org/foobar">http://example.org/foobar</A><BR>
      <A HREF="bar?a&amp;b">http://example.org/foo/bar?a&b</A>
      </BODY>
  )HTML");
  EXPECT_EQ(
      GetDocument().documentElement()->outerHTML(),
      "<html><head><meta name=\"color-scheme\" content=\"light "
      "dark\"></head><body><div class=\"line-gutter-backdrop\"></div><form "
      "autocomplete=\"off\"><label class=\"line-wrap-control\">"
      "<input type=\"checkbox\"></label>"
      "</form><table><tbody><tr><td class=\"line-number\" value=\"1\"></td><td "
      "class=\"line-content\"><br></td></tr><tr><td class=\"line-number\" "
      "value=\"2\"></td><td class=\"line-content\">      <span "
      "class=\"html-tag\">&lt;HEAD&gt;</span><span class=\"html-tag\">&lt;BASE "
      "<span class=\"html-attribute-name\">HREF</span><base "
      "href=\"http://example.org/foo/\">=\"<a class=\"html-attribute-value "
      "html-resource-link\" target=\"_blank\" href=\"http://example.org/foo/\" "
      "rel=\"noreferrer "
      "noopener\">http://example.org/foo/</a>\"&gt;</span><span "
      "class=\"html-tag\">&lt;/HEAD&gt;</span></td></tr><tr><td "
      "class=\"line-number\" value=\"3\"></td><td class=\"line-content\">      "
      "<span class=\"html-tag\">&lt;BODY&gt;</span></td></tr><tr><td "
      "class=\"line-number\" value=\"4\"></td><td class=\"line-content\">      "
      "<span class=\"html-tag\">&lt;A <span "
      "class=\"html-attribute-name\">HREF</span>=\"<a "
      "class=\"html-attribute-value html-external-link\" target=\"_blank\" "
      "href=\"bar\" rel=\"noreferrer "
      "noopener\">bar</a>\"&gt;</span>http://example.org/foo/bar<span "
      "class=\"html-tag\">&lt;/A&gt;</span><span "
      "class=\"html-tag\">&lt;BR&gt;</span></td></tr><tr><td "
      "class=\"line-number\" value=\"5\"></td><td class=\"line-content\">      "
      "<span class=\"html-tag\">&lt;A <span "
      "class=\"html-attribute-name\">HREF</span>=\"<a "
      "class=\"html-attribute-value html-external-link\" target=\"_blank\" "
      "href=\"/bar\" rel=\"noreferrer "
      "noopener\">/bar</a>\"&gt;</span>http://example.org/bar<span "
      "class=\"html-tag\">&lt;/A&gt;</span><span "
      "class=\"html-tag\">&lt;BR&gt;</span></td></tr><tr><td "
      "class=\"line-number\" value=\"6\"></td><td class=\"line-content\">      "
      "<span class=\"html-tag\">&lt;A <span "
      "class=\"html-attribute-name\">HREF</span>=\"<a "
      "class=\"html-attribute-value html-external-link\" target=\"_blank\" "
      "href=\"http://example.org/foobar\" rel=\"noreferrer "
      "noopener\">http://example.org/foobar</a>\"&gt;</span>http://example.org/"
      "foobar<span class=\"html-tag\">&lt;/A&gt;</span><span "
      "class=\"html-tag\">&lt;BR&gt;</span></td></tr><tr><td "
      "class=\"line-number\" value=\"7\"></td><td class=\"line-content\">      "
      "<span class=\"html-tag\">&lt;A <span "
      "class=\"html-attribute-name\">HREF</span>=\"<a "
      "class=\"html-attribute-value html-external-link\" target=\"_blank\" "
      "href=\"bar?a&amp;b\" rel=\"noreferrer "
      "noopener\">bar?a&amp;amp;b</a>\"&gt;</span>http://example.org/foo/"
      "bar?a&amp;b<span class=\"html-tag\">&lt;/A&gt;</span></td></tr><tr><td "
      "class=\"line-number\" value=\"8\"></td><td class=\"line-content\">      "
      "<span class=\"html-tag\">&lt;/BODY&gt;</span></td></tr><tr><td "
      "class=\"line-number\" value=\"9\"></td><td class=\"line-content\">  "
      "<span "
      "class=\"html-end-of-file\"></span></td></tr></tbody></table></body></"
      "html>");
}

TEST_F(HTMLViewSourceDocumentTest, ViewSource5) {
  LoadMainResource(R"HTML(


      <p>

      <input


      type="text">
      </p>

  )HTML");
  EXPECT_EQ(
      GetDocument().documentElement()->outerHTML(),
      "<html><head><meta name=\"color-scheme\" content=\"light "
      "dark\"></head><body><div class=\"line-gutter-backdrop\"></div><form "
      "autocomplete=\"off\"><label class=\"line-wrap-control\">"
      "<input type=\"checkbox\"></label>"
      "</form><table><tbody><tr><td class=\"line-number\" value=\"1\"></td><td "
      "class=\"line-content\"><br></td></tr><tr><td class=\"line-number\" "
      "value=\"2\"></td><td class=\"line-content\"><br></td></tr><tr><td "
      "class=\"line-number\" value=\"3\"></td><td "
      "class=\"line-content\"><br></td></tr><tr><td class=\"line-number\" "
      "value=\"4\"></td><td class=\"line-content\">      <span "
      "class=\"html-tag\">&lt;p&gt;</span></td></tr><tr><td "
      "class=\"line-number\" value=\"5\"></td><td "
      "class=\"line-content\"><br></td></tr><tr><td class=\"line-number\" "
      "value=\"6\"></td><td class=\"line-content\">      <span "
      "class=\"html-tag\">&lt;input</span></td></tr><tr><td "
      "class=\"line-number\" value=\"7\"></td><td "
      "class=\"line-content\"><br></td></tr><tr><td class=\"line-number\" "
      "value=\"8\"></td><td class=\"line-content\"><br></td></tr><tr><td "
      "class=\"line-number\" value=\"9\"></td><td class=\"line-content\">      "
      "<span class=\"html-attribute-name\">type</span>=\"<span "
      "class=\"html-attribute-value\">text</span>\"&gt;</td></tr><tr><td "
      "class=\"line-number\" value=\"10\"></td><td class=\"line-content\">     "
      " <span class=\"html-tag\">&lt;/p&gt;</span></td></tr><tr><td "
      "class=\"line-number\" value=\"11\"></td><td "
      "class=\"line-content\"><br></td></tr><tr><td class=\"line-number\" "
      "value=\"12\"></td><td class=\"line-content\">  <span "
      "class=\"html-end-of-file\"></span></td></tr></tbody></table></body></"
      "html>");
}

TEST_F(HTMLViewSourceDocumentTest, ViewSource6) {
  std::string many_spaces(32760, ' ');
  LoadMainResource((many_spaces + std::string("       <b>A</b>  ")).c_str());
  std::string expected_beginning(
      "<html><head><meta name=\"color-scheme\" content=\"light "
      "dark\"></head><body><div class=\"line-gutter-backdrop\"></div><form "
      "autocomplete=\"off\"><label class=\"line-wrap-control\">"
      "<input type=\"checkbox\"></label>"
      "</form><table><tbody><tr><td class=\"line-number\" value=\"1\">"
      "</td><td class=\"line-content\">      ");
  std::string expected_ending(
      " <span class=\"html-tag\">&lt;b&gt;</span>A<span "
      "class=\"html-tag\">&lt;/b&gt;</span>  <span "
      "class=\"html-end-of-file\"></span></td></tr></tbody></table></body></"
      "html>");
  EXPECT_EQ(GetDocument().documentElement()->outerHTML(),
            (expected_beginning + many_spaces + expected_ending).c_str());
}

TEST_F(HTMLViewSourceDocumentTest, ViewSource7) {
  LoadMainResource("1234567");
  EXPECT_EQ(
      GetDocument().documentElement()->outerHTML(),
      "<html><head><meta name=\"color-scheme\" content=\"light "
      "dark\"></head><body><div class=\"line-gutter-backdrop\"></div><form "
      "autocomplete=\"off\"><label class=\"line-wrap-control\">"
      "<input type=\"checkbox\"></label>"
      "</form><table><tbody><tr><td class=\"line-number\" value=\"1\">"
      "</td><td class=\"line-content\">1234567<span "
      "class=\"html-end-of-file\"></span></td></tr></tbody></table></"
      "body></html>");
}

TEST_F(HTMLViewSourceDocumentTest, ViewSource8) {
  LoadMainResource(R"HTML(
      <!DOCTYPE html>
      <html>
      <body>
      <img src="img.png" />
      <img srcset="img.png, img2.png" />
      <img src="img.png" srcset="img.png 1x, img2.png 2x, img3.png 3x" />
      <img srcset="img.png 480w, img2.png 640w, img3.png 1024w" />
      </body>
      </html>
  )HTML");
  EXPECT_EQ(
      GetDocument().documentElement()->outerHTML(),
      "<html><head><meta name=\"color-scheme\" content=\"light "
      "dark\"></head><body><div class=\"line-gutter-backdrop\"></div><form "
      "autocomplete=\"off\"><label class=\"line-wrap-control\">"
      "<input type=\"checkbox\"></label>"
      "</form><table><tbody><tr><td class=\"line-number\" value=\"1\"></td><td "
      "class=\"line-content\"><br></td></tr><tr><td class=\"line-number\" "
      "value=\"2\"></td><td class=\"line-content\">      <span "
      "class=\"html-doctype\">&lt;!DOCTYPE html&gt;</span></td></tr><tr><td "
      "class=\"line-number\" value=\"3\"></td><td class=\"line-content\">      "
      "<span class=\"html-tag\">&lt;html&gt;</span></td></tr><tr><td "
      "class=\"line-number\" value=\"4\"></td><td class=\"line-content\">      "
      "<span class=\"html-tag\">&lt;body&gt;</span></td></tr><tr><td "
      "class=\"line-number\" value=\"5\"></td><td class=\"line-content\">      "
      "<span class=\"html-tag\">&lt;img <span "
      "class=\"html-attribute-name\">src</span>=\"<a "
      "class=\"html-attribute-value html-resource-link\" target=\"_blank\" "
      "href=\"img.png\" rel=\"noreferrer noopener\">img.png</a>\" "
      "/&gt;</span></td></tr><tr><td class=\"line-number\" "
      "value=\"6\"></td><td class=\"line-content\">      <span "
      "class=\"html-tag\">&lt;img <span "
      "class=\"html-attribute-name\">srcset</span>=\"<a "
      "class=\"html-attribute-value html-resource-link\" target=\"_blank\" "
      "href=\"img.png\" rel=\"noreferrer noopener\">img.png</a>,<a "
      "class=\"html-attribute-value html-resource-link\" target=\"_blank\" "
      "href=\"img2.png\" rel=\"noreferrer noopener\"> img2.png</a>\" "
      "/&gt;</span></td></tr><tr><td class=\"line-number\" "
      "value=\"7\"></td><td class=\"line-content\">      <span "
      "class=\"html-tag\">&lt;img <span "
      "class=\"html-attribute-name\">src</span>=\"<a "
      "class=\"html-attribute-value html-resource-link\" target=\"_blank\" "
      "href=\"img.png\" rel=\"noreferrer noopener\">img.png</a>\" <span "
      "class=\"html-attribute-name\">srcset</span>=\"<a "
      "class=\"html-attribute-value html-resource-link\" target=\"_blank\" "
      "href=\"img.png\" rel=\"noreferrer noopener\">img.png 1x</a>,<a "
      "class=\"html-attribute-value html-resource-link\" target=\"_blank\" "
      "href=\"img2.png\" rel=\"noreferrer noopener\"> img2.png 2x</a>,<a "
      "class=\"html-attribute-value html-resource-link\" target=\"_blank\" "
      "href=\"img3.png\" rel=\"noreferrer noopener\"> img3.png 3x</a>\" "
      "/&gt;</span></td></tr><tr><td class=\"line-number\" "
      "value=\"8\"></td><td class=\"line-content\">      <span "
      "class=\"html-tag\">&lt;img <span "
      "class=\"html-attribute-name\">srcset</span>=\"<a "
      "class=\"html-attribute-value html-resource-link\" target=\"_blank\" "
      "href=\"img.png\" rel=\"noreferrer noopener\">img.png 480w</a>,<a "
      "class=\"html-attribute-value html-resource-link\" target=\"_blank\" "
      "href=\"img2.png\" rel=\"noreferrer noopener\"> img2.png 640w</a>,<a "
      "class=\"html-attribute-value html-resource-link\" target=\"_blank\" "
      "href=\"img3.png\" rel=\"noreferrer noopener\"> img3.png 1024w</a>\" "
      "/&gt;</span></td></tr><tr><td class=\"line-number\" "
      "value=\"9\"></td><td class=\"line-content\">      <span "
      "class=\"html-tag\">&lt;/body&gt;</span></td></tr><tr><td "
      "class=\"line-number\" value=\"10\"></td><td class=\"line-content\">     "
      " <span class=\"html-tag\">&lt;/html&gt;</span></td></tr><tr><td "
      "class=\"line-number\" value=\"11\"></td><td class=\"line-content\">  "
      "<span "
      "class=\"html-end-of-file\"></span></td></tr></tbody></table></body></"
      "html>");
}

TEST_F(HTMLViewSourceDocumentTest, ViewSource9) {
  LoadMainResource(R"HTML(
      <!DOCTYPE html>
      <head>
      <title>Test</title>
      <script type="text/javascript">
      "<!--  --!><script>";
  )HTML");
  EXPECT_EQ(
      GetDocument().documentElement()->outerHTML(),
      "<html><head><meta name=\"color-scheme\" content=\"light "
      "dark\"></head><body><div class=\"line-gutter-backdrop\"></div><form "
      "autocomplete=\"off\"><label class=\"line-wrap-control\">"
      "<input type=\"checkbox\"></label>"
      "</form><table><tbody><tr><td class=\"line-number\" value=\"1\"></td><td "
      "class=\"line-content\"><br></td></tr><tr><td class=\"line-number\" "
      "value=\"2\"></td><td class=\"line-content\">      <span "
      "class=\"html-doctype\">&lt;!DOCTYPE html&gt;</span></td></tr><tr><td "
      "class=\"line-number\" value=\"3\"></td><td class=\"line-content\">      "
      "<span class=\"html-tag\">&lt;head&gt;</span></td></tr><tr><td "
      "class=\"line-number\" value=\"4\"></td><td class=\"line-content\">      "
      "<span class=\"html-tag\">&lt;title&gt;</span>Test<span "
      "class=\"html-tag\">&lt;/title&gt;</span></td></tr><tr><td "
      "class=\"line-number\" value=\"5\"></td><td class=\"line-content\">      "
      "<span class=\"html-tag\">&lt;script <span "
      "class=\"html-attribute-name\">type</span>=\"<span "
      "class=\"html-attribute-value\">text/javascript</span>\"&gt;</span></"
      "td></tr><tr><td class=\"line-number\" value=\"6\"></td><td "
      "class=\"line-content\">      \"&lt;!--  "
      "--!&gt;&lt;script&gt;\";</td></tr><tr><td class=\"line-number\" "
      "value=\"7\"></td><td class=\"line-content\">  <span "
      "class=\"html-end-of-file\"></span></td></tr></tbody></table></body></"
      "html>");
}

TEST_F(HTMLViewSourceDocumentTest, IncompleteToken) {
  LoadMainResource(R"HTML(
      Incomplete token test
      text <h1 there! This text will never make it into a token.
      But it should be in view-source.
  )HTML");
  EXPECT_EQ(
      GetDocument().documentElement()->outerHTML(),
      "<html><head><meta name=\"color-scheme\" content=\"light "
      "dark\"></head><body><div class=\"line-gutter-backdrop\"></div><form "
      "autocomplete=\"off\"><label class=\"line-wrap-control\">"
      "<input type=\"checkbox\"></label>"
      "</form><table><tbody><tr><td class=\"line-number\" value=\"1\"></td><td "
      "class=\"line-content\"><br></td></tr><tr><td class=\"line-number\" "
      "value=\"2\"></td><td class=\"line-content\">      Incomplete token "
      "test</td></tr><tr><td class=\"line-number\" value=\"3\"></td><td "
      "class=\"line-content\">      text <span "
      "class=\"html-end-of-file\">&lt;h1 there! This text will never make it "
      "into a token.</span></td></tr><tr><td class=\"line-number\" "
      "value=\"4\"></td><td class=\"line-content\"><span "
      "class=\"html-end-of-file\">      But it should be in "
      "view-source.</span></td></tr><tr><td class=\"line-number\" "
      "value=\"5\"></td><td class=\"line-content\"><span "
      "class=\"html-end-of-file\">  "
      "</span></td></tr></tbody></table></body></html>");
}

TEST_F(HTMLViewSourceDocumentTest, UnfinishedTextarea) {
  LoadMainResource(R"HTML(<textarea>foobar in textarea
  )HTML");
  EXPECT_EQ(
      GetDocument().documentElement()->outerHTML(),
      "<html><head><meta name=\"color-scheme\" content=\"light "
      "dark\"></head><body><div class=\"line-gutter-backdrop\"></div><form "
      "autocomplete=\"off\"><label class=\"line-wrap-control\">"
      "<input type=\"checkbox\"></label>"
      "</form><table><tbody><tr><td class=\"line-number\" value=\"1\"></td>"
      "<td class=\"line-content\"><span "
      "class=\"html-tag\">&lt;textarea&gt;</span>foobar in "
      "textarea</td></tr><tr><td class=\"line-number\" value=\"2\"></td><td "
      "class=\"line-content\">  <span "
      "class=\"html-end-of-file\"></span></td></tr></tbody></table></body></"
      "html>");
}

TEST_F(HTMLViewSourceDocumentTest, UnfinishedScript) {
  LoadMainResource(R"HTML(<script>foobar in script
  )HTML");
  EXPECT_EQ(
      GetDocument().documentElement()->outerHTML(),
      "<html><head><meta name=\"color-scheme\" content=\"light "
      "dark\"></head><body><div class=\"line-gutter-backdrop\"></div><form "
      "autocomplete=\"off\"><label class=\"line-wrap-control\">"
      "<input type=\"checkbox\"></label>"
      "</form><table><tbody><tr><td class=\"line-number\" value=\"1\"></td>"
      "<td class=\"line-content\"><span "
      "class=\"html-tag\">&lt;script&gt;</span>foobar in "
      "script</td></tr><tr><td class=\"line-number\" value=\"2\"></td><td "
      "class=\"line-content\">  <span "
      "class=\"html-end-of-file\"></span></td></tr></tbody></table></body></"
      "html>");
}

TEST_F(HTMLViewSourceDocumentTest, Linebreak) {
  LoadMainResource("<html>\nR\n\rN\n\nNR\n\n\rRN\n\r\n</html>");
  EXPECT_EQ(
      GetDocument().documentElement()->outerHTML(),
      "<html><head><meta name=\"color-scheme\" content=\"light dark\"></head>"
      "<body><div class=\"line-gutter-backdrop\"></div>"
      "<form autocomplete=\"off\"><label class=\"line-wrap-control\">"
      "<input type=\"checkbox\"></label></form>"
      "<table><tbody>"
      "<tr><td class=\"line-number\" value=\"1\"></td>"
      "<td class=\"line-content\">"
      "<span class=\"html-tag\">&lt;html&gt;</span></td></tr>"
      "<tr><td class=\"line-number\" value=\"2\"></td>"
      "<td class=\"line-content\">R</td></tr>"  // \r -> 1 linebreak
      "<tr><td class=\"line-number\" value=\"3\"></td>"
      "<td class=\"line-content\"><br></td></tr>"
      "<tr><td class=\"line-number\" value=\"4\"></td>"
      "<td class=\"line-content\">N</td></tr>"  // \n -> 1 linebraek
      "<tr><td class=\"line-number\" value=\"5\"></td>"
      "<td class=\"line-content\"><br></td></tr><tr>"
      "<td class=\"line-number\" value=\"6\"></td>"
      "<td class=\"line-content\">NR</td></tr>"  // \n\r -> 2 linebreaks
      "<tr><td class=\"line-number\" value=\"7\"></td>"
      "<td class=\"line-content\"><br></td></tr>"
      "<tr><td class=\"line-number\" value=\"8\"></td>"
      "<td class=\"line-content\"><br></td></tr>"
      "<tr><td class=\"line-number\" value=\"9\"></td>"
      "<td class=\"line-content\">RN</td></tr>"  // \r\n -> 1 linebreak
      "<tr><td class=\"line-number\" value=\"10\"></td>"
      "<td class=\"line-content\"><br></td></tr>"
      "<tr><td class=\"line-number\" value=\"11\"></td>"
      "<td class=\"line-content\">"
      "<span class=\"html-tag\">&lt;/html&gt;</span>"
      "<span class=\"html-end-of-file\"></span>"
      "</td></tr></tbody></table></body></html>");
}

TEST_F(HTMLViewSourceDocumentTest, DOMParts) {
  LoadMainResource(
      R"HTML(<div parseparts>{{#}}foo{{/}}<span {{}}>bar</span></div>)HTML");
  EXPECT_EQ(
      GetDocument().documentElement()->outerHTML(),
      "<html><head><meta name=\"color-scheme\" content=\"light "
      "dark\"></head><body><div class=\"line-gutter-backdrop\"></div><form "
      "autocomplete=\"off\"><label class=\"line-wrap-control\"><input "
      "type=\"checkbox\"></label></form><table><tbody><tr><td "
      "class=\"line-number\" value=\"1\"></td><td class=\"line-content\"><span "
      "class=\"html-tag\">&lt;div <span "
      "class=\"html-attribute-name\">parseparts</span>&gt;</span>{{#}}foo{{/"
      "}}<span class=\"html-tag\">&lt;span <span "
      "class=\"html-attribute-name\">{{}}</span>&gt;</span>bar<span "
      "class=\"html-tag\">&lt;/span&gt;</span><span "
      "class=\"html-tag\">&lt;/div&gt;</span><span "
      "class=\"html-end-of-file\"></span></td></tr></tbody></table></body></"
      "html>");
}

}  // namespace blink
