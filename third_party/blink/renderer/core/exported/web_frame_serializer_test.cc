/*
 * Copyright (C) 2017 Google Inc. All rights reserved.
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

#include <string>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-blink.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_frame_serializer_impl.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_loader_mock_factory.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/kurl_hash.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

class TestFrameHTMLSerializerHandler final
    : public mojom::blink::FrameHTMLSerializerHandler {
 public:
  TestFrameHTMLSerializerHandler() = default;

  mojo::PendingRemote<mojom::blink::FrameHTMLSerializerHandler>
  BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void WaitForDone() {
    test::RunPendingTasks();
    EXPECT_TRUE(done_);
  }

  String ToString() { return builder_.ToString(); }

 private:
  void DidReceiveData(const std::string& data_buffer) final {
    EXPECT_FALSE(done_);
    builder_.Append(base::as_byte_span(data_buffer));
  }

  void Done() final {
    EXPECT_FALSE(done_);
    done_ = true;
  }

  mojo::Receiver<mojom::blink::FrameHTMLSerializerHandler> receiver_{this};
  StringBuilder builder_;
  bool done_ = false;
};

}  // namespace

class WebFrameSerializerTest : public testing::Test {
 protected:
  WebFrameSerializerTest() { helper_.Initialize(); }

  ~WebFrameSerializerTest() override {
    url_test_helpers::UnregisterAllURLsAndClearMemoryCache();
  }

  void RegisterMockedImageURLLoad(const String& url) {
    // Image resources need to be mocked, but irrelevant here what image they
    // map to.
    RegisterMockedFileURLLoad(url_test_helpers::ToKURL(url.Utf8().c_str()),
                              "frameserialization/awesome.png");
  }

  void RegisterMockedFileURLLoad(const KURL& url,
                                 const String& file_path,
                                 const String& mime_type = "image/png") {
    // TODO(crbug.com/751425): We should use the mock functionality
    // via |helper_|.
    url_test_helpers::RegisterMockedURLLoad(
        url, test::CoreTestDataPath(file_path.Utf8().c_str()), mime_type);
  }

  String SerializeFrame(
      LocalFrame* frame,
      const HashMap<KURL, base::FilePath>& url_map,
      const HashMap<FrameToken, base::FilePath>& frame_token_map,
      bool save_with_empty_url) {
    TestFrameHTMLSerializerHandler handler;
    WebFrameSerializerImpl serializer(frame, url_map, frame_token_map,
                                      save_with_empty_url,
                                      handler.BindNewPipeAndPassRemote());
    serializer.Serialize();
    handler.WaitForDone();
    return handler.ToString();
  }

  String SerializeFile(const String& url,
                       const String& file_name,
                       bool save_with_empty_url,
                       const String& mime_type = "text/html") {
    KURL parsed_url(url);
    String file_path("frameserialization/" + file_name);
    RegisterMockedFileURLLoad(parsed_url, file_path, mime_type);
    frame_test_helpers::LoadFrame(MainFrameImpl(), url.Utf8().c_str());
    HashMap<KURL, base::FilePath> url_map;
    url_map.insert(parsed_url, base::FilePath(FILE_PATH_LITERAL("local")));
    return SerializeFrame(MainFrameImpl()->GetFrame(), url_map, {},
                          save_with_empty_url);
  }

  WebLocalFrameImpl* MainFrameImpl() { return helper_.LocalMainFrame(); }

 private:
  test::TaskEnvironment task_environment_;
  frame_test_helpers::WebViewHelper helper_;
};

TEST_F(WebFrameSerializerTest, URLAttributeValues) {
  RegisterMockedImageURLLoad("javascript:\"");

  const char* expected_html =
      "\n<!-- saved from url=(0020)http://www.test.com/ -->\n"
      "<html><head><meta http-equiv=\"Content-Type\" content=\"text/html; "
      "charset=UTF-8\">\n"
      "</head><body><img src=\"javascript:&quot;\">\n"
      "<a href=\"http://www.test.com/local#%22\">local</a>\n"
      "<a "
      "href=\"http://www.example.com/#%22%3E%3Cscript%3Ealert(0)%3C/"
      "script%3E\">external</a>\n"
      "</body></html>";
  String actual_html =
      SerializeFile("http://www.test.com", "url_attribute_values.html", false);
  EXPECT_EQ(expected_html, actual_html);
}

TEST_F(WebFrameSerializerTest, EncodingAndNormalization) {
  const char* expected_html =
      "<!DOCTYPE html>\n"
      "<!-- saved from url=(0020)http://www.test.com/ -->\n"
      "<html><head><meta http-equiv=\"Content-Type\" content=\"text/html; "
      "charset=EUC-KR\">\n"
      "<title>Ensure NFC normalization is not performed by frame "
      "serializer</title>\n"
      "</head><body>\n"
      "\xe4\xc5\xd1\xe2\n"
      "\n</body></html>";
  String actual_html = SerializeFile("http://www.test.com",
                                     "encoding_normalization.html", false);
  EXPECT_EQ(expected_html, actual_html);
}

TEST_F(WebFrameSerializerTest, FromUrlWithMinusMinus) {
  String actual_html =
      SerializeFile("http://www.test.com?--x--", "text_only_page.html", false);
  EXPECT_EQ("<!-- saved from url=(0030)http://www.test.com/?-%2Dx-%2D -->",
            actual_html.substr(1, 60));
}

TEST_F(WebFrameSerializerTest, WithoutFrameUrl) {
  const char* expected_html =
      "<!DOCTYPE html>\n"
      "<!-- saved from url=(0014)about:internet -->\n"
      "<html><head><meta http-equiv=\"Content-Type\" content=\"text/html; "
      "charset=EUC-KR\">\n"
      "<title>Ensure NFC normalization is not performed by frame "
      "serializer</title>\n"
      "</head><body>\n"
      "\xe4\xc5\xd1\xe2\n"
      "\n</body></html>";
  String actual_html =
      SerializeFile("http://www.test.com", "encoding_normalization.html", true);
  EXPECT_EQ(expected_html, actual_html);
}

TEST_F(WebFrameSerializerTest, ShadowDOM) {
  const char* expected_html = R"HTML(<!DOCTYPE html>
<!-- saved from url=(0014)about:internet -->
<html><head><meta http-equiv="Content-Type" content="text/html; charset=windows-1252"></head><body>
<div id="host1"><template shadowrootmode="open">
    <div>hello world</div>
  </template>
  
</div>
<div id="host2"><template shadowrootmode="closed">
    <div>hello world</div>
  </template>
  
</div>
<div id="host3"><template shadowrootmode="open" shadowrootdelegatesfocus>
    <div>hello world</div>
  </template>
  
</div>
<div id="host4"><template shadowrootmode="open">
    <slot></slot>
  </template>
  
  <div>light dom slotted</div>
</div>
<div id="host5"><template shadowrootmode="open"><div>hello world</div></template>
  <div>light dom</div>
</div>
<script>
host5.attachShadow({mode: 'open'}).innerHTML = '<div>hello world</div>';
</script>
<div id="host6"><template shadowrootmode="open"><div>hello world</div></template></div>
<script>
host6.attachShadow({mode: 'open'}).innerHTML = '<div>hello world</div>';
</script>
<div id="host7"><template shadowrootmode="open"></template></div>
</body></html>)HTML";
  String actual_html =
      SerializeFile("http://www.test.com", "shadowdom.html", true);
  EXPECT_EQ(String(expected_html), actual_html);
}

TEST_F(WebFrameSerializerTest, EntitiesInAttributeHtml) {
  String expected(
      u"<body "
      u"title=\"&quot;&amp;&lt;&gt;'\u00A9&nbsp;\t\n\r\">\n\n</body></html>");

  String actual =
      SerializeFile("http://www.test.com/2", "entities.html", false);
  EXPECT_TRUE(actual.ends_with(expected));
}

TEST_F(WebFrameSerializerTest, EntitiesInAttributeXml) {
  String expected(
      u"<body "
      u"title=\"&quot;&amp;&lt;&gt;'\u00C2\u00A9\u00C2\u00A0&#9;&#10;&#13;\" "
      u"/>\n</html>");

  String actual = SerializeFile("http://www.test.com/2", "entities.xml", false,
                                "application/xhtml+xml");
  EXPECT_TRUE(actual.ends_with(expected));
}

TEST_F(WebFrameSerializerTest, RewriteLinksAndFrameSources) {
  RegisterMockedImageURLLoad("http://www.test.com/img.png");
  frame_test_helpers::LoadHTMLString(
      MainFrameImpl(),
      "<!DOCTYPE html><html><body>"
      "<img src=\"img.png\">"
      "<a href=\"other.html\">other</a>"
      "<iframe srcdoc=\"<p>child</p>\"></iframe>"
      "</body></html>",
      url_test_helpers::ToKURL("http://www.test.com/page.html"));

  Frame* child_frame = MainFrameImpl()->GetFrame()->Tree().FirstChild();
  ASSERT_TRUE(child_frame);

  HashMap<KURL, base::FilePath> url_map;
  url_map.insert(KURL("http://www.test.com/img.png"),
                 base::FilePath(FILE_PATH_LITERAL("page_files"))
                     .Append(FILE_PATH_LITERAL("img.png")));

  HashMap<FrameToken, base::FilePath> frame_token_map;
  frame_token_map.insert(child_frame->GetFrameToken(),
                         base::FilePath(FILE_PATH_LITERAL("page_files"))
                             .Append(FILE_PATH_LITERAL("child.html")));

  String actual_html = SerializeFrame(MainFrameImpl()->GetFrame(), url_map,
                                      frame_token_map, false);
  EXPECT_NE(actual_html.find("<img src=\"./page_files/img.png\">"),
            String::npos);
  EXPECT_NE(actual_html.find("<a href=\"http://www.test.com/other.html\">"),
            String::npos);
  EXPECT_NE(actual_html.find("<iframe src=\"./page_files/child.html\">"),
            String::npos);
  EXPECT_EQ(actual_html.find("srcdoc"), String::npos);
}

}  // namespace blink
