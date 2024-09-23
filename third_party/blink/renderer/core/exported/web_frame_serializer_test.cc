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

#include "third_party/blink/public/web/web_frame_serializer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_frame_serializer_client.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_loader_mock_factory.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

class SimpleWebFrameSerializerClient final : public WebFrameSerializerClient {
 public:
  String ToString() { return builder_.ToString(); }

 private:
  void DidSerializeDataForFrame(const WebVector<char>& data,
                                FrameSerializationStatus) final {
    builder_.Append(data.data(), static_cast<unsigned>(data.size()));
  }

  StringBuilder builder_;
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

  class SingleLinkRewritingDelegate
      : public WebFrameSerializer::LinkRewritingDelegate {
   public:
    SingleLinkRewritingDelegate(const WebURL& url, const WebString& local_path)
        : url_(url), local_path_(local_path) {}

    bool RewriteFrameSource(WebFrame* frame,
                            WebString* rewritten_link) override {
      return false;
    }

    bool RewriteLink(const WebURL& url, WebString* rewritten_link) override {
      if (url != url_)
        return false;

      *rewritten_link = local_path_;
      return true;
    }

   private:
    const WebURL url_;
    const WebString local_path_;
  };

  String SerializeFile(const String& url,
                       const String& file_name,
                       bool save_with_empty_url) {
    KURL parsed_url(url);
    String file_path("frameserialization/" + file_name);
    RegisterMockedFileURLLoad(parsed_url, file_path, "text/html");
    frame_test_helpers::LoadFrame(MainFrameImpl(), url.Utf8().c_str());
    SingleLinkRewritingDelegate delegate(parsed_url, WebString("local"));
    SimpleWebFrameSerializerClient serializer_client;
    WebFrameSerializer::Serialize(MainFrameImpl(), &serializer_client,
                                  &delegate, save_with_empty_url);
    return serializer_client.ToString();
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
            actual_html.Substring(1, 60));
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

}  // namespace blink
