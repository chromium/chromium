// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/message_loop/message_loop_current.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/frame_load_waiter.h"
#include "content/public/test/test_utils.h"
#include "content/renderer/savable_resources.h"
#include "content/shell/browser/shell.h"
#include "net/base/filename_util.h"
#include "net/url_request/url_request_context.h"
#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_element_collection.h"
#include "third_party/blink/public/web/web_frame_content_dumper.h"
#include "third_party/blink/public/web/web_frame_serializer.h"
#include "third_party/blink/public/web/web_frame_serializer_client.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_meta_element.h"
#include "third_party/blink/public/web/web_node.h"
#include "third_party/blink/public/web/web_view.h"

using blink::WebData;
using blink::WebDocument;
using blink::WebElement;
using blink::WebMetaElement;
using blink::WebElementCollection;
using blink::WebFrame;
using blink::WebFrameSerializer;
using blink::WebFrameSerializerClient;
using blink::WebLocalFrame;
using blink::WebNode;
using blink::WebString;
using blink::WebURL;
using blink::WebView;
using blink::WebVector;

namespace content {

bool HasDocType(const WebDocument& doc) {
  return doc.FirstChild().IsDocumentTypeNode();
}

// https://crbug.com/788788
#if defined(OS_ANDROID) && defined(ADDRESS_SANITIZER)
#define MAYBE_DomSerializerTests DISABLED_DomSerializerTests
#else
#define MAYBE_DomSerializerTests DomSerializerTests
#endif  // defined(OS_ANDROID) && defined(ADDRESS_SANITIZER)
class MAYBE_DomSerializerTests : public ContentBrowserTest,
                                 public WebFrameSerializerClient {
 public:
  MAYBE_DomSerializerTests() : serialization_reported_end_of_data_(false) {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kSingleProcess);
#if defined(OS_WIN)
    // Don't want to try to create a GPU process.
    command_line->AppendSwitch(switches::kDisableGpu);
#endif
  }

  void SetUpOnMainThread() override {
    render_view_routing_id_ =
        shell()->web_contents()->GetRenderViewHost()->GetRoutingID();
  }

  // DomSerializerDelegate.
  void DidSerializeDataForFrame(const WebVector<char>& data,
                                FrameSerializationStatus status) override {
    // Check finish status of current frame.
    ASSERT_FALSE(serialization_reported_end_of_data_);

    // Add data to corresponding frame's content.
    serialized_contents_.append(data.Data(), data.size());

    // Current frame is completed saving, change the finish status.
    if (status == WebFrameSerializerClient::kCurrentFrameIsFinished)
      serialization_reported_end_of_data_ = true;
  }

  RenderView* GetRenderView() {
    return RenderView::FromRoutingID(render_view_routing_id_);
  }

  WebView* GetWebView() {
    return GetRenderView()->GetWebView();
  }

  WebLocalFrame* GetMainFrame() {
    return GetRenderView()->GetMainRenderFrame()->GetWebFrame();
  }

  WebLocalFrame* FindSubFrameByURL(const GURL& url) {
    for (WebFrame* frame = GetWebView()->MainFrame(); frame;
         frame = frame->TraverseNext()) {
      DCHECK(frame->IsWebLocalFrame());
      if (GURL(frame->ToWebLocalFrame()->GetDocument().Url()) == url)
        return frame->ToWebLocalFrame();
    }
    return nullptr;
  }

  // Load web page according to input content and relative URLs within
  // the document.
  void LoadContents(const std::string& contents,
                    const GURL& base_url,
                    const WebString encoding_info) {
    FrameLoadWaiter waiter(GetRenderView()->GetMainRenderFrame());
    GetRenderView()->GetMainRenderFrame()->LoadHTMLString(
        contents, base_url,
        encoding_info.IsEmpty() ? "UTF-8" : encoding_info.Utf8(), GURL(),
        false /* replace_current_item */);
    waiter.Wait();
  }

  class SingleLinkRewritingDelegate
      : public WebFrameSerializer::LinkRewritingDelegate {
   public:
    SingleLinkRewritingDelegate(const WebURL& url, const WebString& localPath)
        : url_(url), local_path_(localPath) {}

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

  // Serialize DOM belonging to a frame with the specified |frame_url|.
  void SerializeDomForURL(const GURL& frame_url) {
    SerializeDomForURL(frame_url, false);
  }

  void SerializeDomForURL(const GURL& frame_url, bool save_with_empty_url) {
    // Find corresponding WebFrame according to frame_url.
    WebFrame* web_frame = FindSubFrameByURL(frame_url);
    ASSERT_TRUE(web_frame != nullptr);
    WebString file_path = WebString::FromUTF8("c:\\dummy.htm");
    SingleLinkRewritingDelegate delegate(frame_url, file_path);
    // Start serializing DOM.
    bool result = WebFrameSerializer::Serialize(
        web_frame->ToWebLocalFrame(), this, &delegate, save_with_empty_url);
    ASSERT_TRUE(result);
  }

  void SerializeHTMLDOMWithDocTypeOnRenderer(const GURL& file_url) {
    // Make sure original contents have document type.
    WebLocalFrame* web_frame = FindSubFrameByURL(file_url);
    ASSERT_TRUE(web_frame != nullptr);
    WebDocument doc = web_frame->GetDocument();
    ASSERT_TRUE(HasDocType(doc));
    // Do serialization.
    SerializeDomForURL(file_url);
    // Load the serialized contents.
    ASSERT_TRUE(serialization_reported_end_of_data_);
    LoadContents(serialized_contents_, file_url,
                 web_frame->GetDocument().Encoding());
    // Make sure serialized contents still have document type.
    web_frame = GetMainFrame();
    doc = web_frame->GetDocument();
    ASSERT_TRUE(HasDocType(doc));
  }

  void SerializeHTMLDOMWithoutDocTypeOnRenderer(const GURL& file_url) {
    // Make sure original contents do not have document type.
    WebLocalFrame* web_frame = FindSubFrameByURL(file_url);
    ASSERT_TRUE(web_frame != nullptr);
    WebDocument doc = web_frame->GetDocument();
    ASSERT_TRUE(!HasDocType(doc));
    // Do serialization.
    SerializeDomForURL(file_url);
    // Load the serialized contents.
    ASSERT_TRUE(serialization_reported_end_of_data_);
    LoadContents(serialized_contents_, file_url,
                 web_frame->GetDocument().Encoding());
    // Make sure serialized contents do not have document type.
    web_frame = GetMainFrame();
    doc = web_frame->GetDocument();
    ASSERT_TRUE(!HasDocType(doc));
  }

  void SerializeXMLDocWithBuiltInEntitiesOnRenderer(
      const GURL& xml_file_url, const std::string& original_contents) {
    // Do serialization.
    SerializeDomForURL(xml_file_url);
    // Compare the serialized contents with original contents.
    ASSERT_TRUE(serialization_reported_end_of_data_);
    ASSERT_EQ(original_contents, serialized_contents_);
  }

  void SerializeHTMLDOMWithAddingMOTWOnRenderer(
      const GURL& file_url,
      const std::string& original_contents,
      bool save_with_empty_url) {
    // Make sure original contents does not have MOTW;
    GURL frame_url = save_with_empty_url ? GURL("about:internet") : file_url;
    std::string motw_declaration =
        WebFrameSerializer::GenerateMarkOfTheWebDeclaration(frame_url).Utf8();
    ASSERT_FALSE(motw_declaration.empty());
    // The encoding of original contents is ISO-8859-1, so we convert the MOTW
    // declaration to ASCII and search whether original contents has it or not.
    ASSERT_TRUE(std::string::npos == original_contents.find(motw_declaration));

    // Do serialization.
    SerializeDomForURL(file_url, save_with_empty_url);
    // Make sure the serialized contents have MOTW ;
    ASSERT_TRUE(serialization_reported_end_of_data_);
    ASSERT_FALSE(std::string::npos ==
                 serialized_contents_.find(motw_declaration));
  }

  void SerializeHTMLDOMWithNoMetaCharsetInOriginalDocOnRenderer(
      const GURL& file_url) {
    // Make sure there is no META charset declaration in original document.
    WebLocalFrame* web_frame = FindSubFrameByURL(file_url);
    ASSERT_TRUE(web_frame != nullptr);
    WebDocument doc = web_frame->GetDocument();
    ASSERT_TRUE(doc.IsHTMLDocument());
    WebElement head_element = doc.Head();
    ASSERT_TRUE(!head_element.IsNull());
    // Go through all children of HEAD element.
    WebElementCollection meta_elements =
        head_element.GetElementsByHTMLTagName("meta");
    for (WebElement element = meta_elements.FirstItem(); !element.IsNull();
         element = meta_elements.NextItem()) {
      ASSERT_TRUE(element.To<WebMetaElement>().ComputeEncoding().IsEmpty());
    }
    // Do serialization.
    SerializeDomForURL(file_url);

    // Load the serialized contents.
    ASSERT_TRUE(serialization_reported_end_of_data_);
    LoadContents(serialized_contents_, file_url,
                 web_frame->GetDocument().Encoding());
    // Make sure the first child of HEAD element is META which has charset
    // declaration in serialized contents.
    web_frame = GetMainFrame();
    ASSERT_TRUE(web_frame != nullptr);
    doc = web_frame->GetDocument();
    ASSERT_TRUE(doc.IsHTMLDocument());
    head_element = doc.Head();
    ASSERT_TRUE(!head_element.IsNull());
    ASSERT_TRUE(!head_element.FirstChild().IsNull());
    ASSERT_TRUE(head_element.FirstChild().IsElementNode());
    WebMetaElement meta_element =
        head_element.FirstChild().To<WebMetaElement>();
    ASSERT_EQ(meta_element.ComputeEncoding(),
              web_frame->GetDocument().Encoding());

    // Make sure no more additional META tags which have charset declaration.
    meta_elements = head_element.GetElementsByHTMLTagName("meta");
    for (WebElement element = meta_elements.FirstItem(); !element.IsNull();
         element = meta_elements.NextItem()) {
      if (element == meta_element)
        continue;
      ASSERT_TRUE(element.To<WebMetaElement>().ComputeEncoding().IsEmpty());
    }
  }

  void SerializeHTMLDOMWithMultipleMetaCharsetInOriginalDocOnRenderer(
      const GURL& file_url) {
    // Make sure there are multiple META charset declarations in original
    // document.
    WebLocalFrame* web_frame = FindSubFrameByURL(file_url);
    ASSERT_TRUE(web_frame != nullptr);
    WebDocument doc = web_frame->GetDocument();
    ASSERT_TRUE(doc.IsHTMLDocument());
    WebElement head_element = doc.Head();
    ASSERT_TRUE(!head_element.IsNull());
    // Go through all children of HEAD element.
    int charset_declaration_count = 0;
    WebElementCollection meta_elements =
        head_element.GetElementsByHTMLTagName("meta");
    for (WebElement element = meta_elements.FirstItem(); !element.IsNull();
         element = meta_elements.NextItem()) {
      if (!element.To<WebMetaElement>().ComputeEncoding().IsEmpty())
        ++charset_declaration_count;
    }
    // The original doc has more than META tags which have charset declaration.
    ASSERT_GT(charset_declaration_count, 1);

    // Do serialization.
    SerializeDomForURL(file_url);

    // Load the serialized contents.
    ASSERT_TRUE(serialization_reported_end_of_data_);
    LoadContents(serialized_contents_, file_url,
                 web_frame->GetDocument().Encoding());
    // Make sure only first child of HEAD element is META which has charset
    // declaration in serialized contents.
    web_frame = GetMainFrame();
    ASSERT_TRUE(web_frame != nullptr);
    doc = web_frame->GetDocument();
    ASSERT_TRUE(doc.IsHTMLDocument());
    head_element = doc.Head();
    ASSERT_TRUE(!head_element.IsNull());
    ASSERT_TRUE(!head_element.FirstChild().IsNull());
    ASSERT_TRUE(head_element.FirstChild().IsElementNode());
    WebMetaElement meta_element =
        head_element.FirstChild().To<WebMetaElement>();
    ASSERT_EQ(meta_element.ComputeEncoding(),
              web_frame->GetDocument().Encoding());

    // Make sure no more additional META tags which have charset declaration.
    meta_elements = head_element.GetElementsByHTMLTagName("meta");
    for (WebElement element = meta_elements.FirstItem(); !element.IsNull();
         element = meta_elements.NextItem()) {
      if (element == meta_element)
        continue;
      ASSERT_TRUE(element.To<WebMetaElement>().ComputeEncoding().IsEmpty());
    }
  }

  void SerializeHTMLDOMWithEntitiesInTextOnRenderer() {
    base::FilePath page_file_path = GetTestFilePath(
        "dom_serializer", "dom_serializer/htmlentities_in_text.htm");
    // Get file URL. The URL is dummy URL to identify the following loading
    // actions. The test content is in constant:original_contents.
    GURL file_url = net::FilePathToFileURL(page_file_path);
    ASSERT_TRUE(file_url.SchemeIsFile());
    // Test contents.
    static const char* const original_contents =
        "<html><body>&amp;&lt;&gt;\"\'</body></html>";
    // Load the test contents.
    LoadContents(original_contents, file_url, WebString());

    // Get BODY's text content in DOM.
    WebLocalFrame* web_frame = FindSubFrameByURL(file_url);
    ASSERT_TRUE(web_frame != nullptr);
    WebDocument doc = web_frame->GetDocument();
    ASSERT_TRUE(doc.IsHTMLDocument());
    WebElement body_ele = doc.Body();
    ASSERT_TRUE(!body_ele.IsNull());
    WebNode text_node = body_ele.FirstChild();
    ASSERT_TRUE(text_node.IsTextNode());
    ASSERT_EQ(text_node.NodeValue().Utf8(), "&<>\"\'");
    // Do serialization.
    SerializeDomForURL(file_url);
    // Compare the serialized contents with original contents.
    ASSERT_TRUE(serialization_reported_end_of_data_);
    // Compare the serialized contents with original contents to make sure
    // they are same.
    // Because we add MOTW when serializing DOM, so before comparison, we also
    // need to add MOTW to original_contents.
    std::string original_str =
        WebFrameSerializer::GenerateMarkOfTheWebDeclaration(file_url).Utf8();
    original_str += original_contents;
    // Since WebCore now inserts a new HEAD element if there is no HEAD element
    // when creating BODY element. (Please see
    // HTMLParser::bodyCreateErrorCheck.) We need to append the HEAD content and
    // corresponding META content if we find WebCore-generated HEAD element.
    if (!doc.Head().IsNull()) {
      WebString encoding = web_frame->GetDocument().Encoding();
      std::string htmlTag("<html>");
      std::string::size_type pos = original_str.find(htmlTag);
      ASSERT_NE(std::string::npos, pos);
      pos += htmlTag.length();
      std::string head_part("<head>");
      head_part +=
          WebFrameSerializer::GenerateMetaCharsetDeclaration(encoding).Utf8();
      head_part += "</head>";
      original_str.insert(pos, head_part);
    }
    ASSERT_EQ(original_str, serialized_contents_);
  }

  void SerializeHTMLDOMWithEntitiesInAttributeValueOnRenderer() {
    base::FilePath page_file_path = GetTestFilePath(
        "dom_serializer", "dom_serializer/htmlentities_in_attribute_value.htm");
    // Get file URL. The URL is dummy URL to identify the following loading
    // actions. The test content is in constant:original_contents.
    GURL file_url = net::FilePathToFileURL(page_file_path);
    ASSERT_TRUE(file_url.SchemeIsFile());
    // Test contents.
    static const char* const original_contents =
        "<html><body title=\"&amp;&lt;&gt;&quot;&#39;\"></body></html>";
    // Load the test contents.
    LoadContents(original_contents, file_url, WebString());
    // Get value of BODY's title attribute in DOM.
    WebLocalFrame* web_frame = FindSubFrameByURL(file_url);
    ASSERT_TRUE(web_frame != nullptr);
    WebDocument doc = web_frame->GetDocument();
    ASSERT_TRUE(doc.IsHTMLDocument());
    WebElement body_ele = doc.Body();
    ASSERT_TRUE(!body_ele.IsNull());
    WebString value = body_ele.GetAttribute("title");
    ASSERT_EQ(value.Utf8(), "&<>\"\'");
    // Do serialization.
    SerializeDomForURL(file_url);
    // Compare the serialized contents with original contents.
    ASSERT_TRUE(serialization_reported_end_of_data_);
    // Compare the serialized contents with original contents to make sure
    // they are same.
    std::string original_str =
        WebFrameSerializer::GenerateMarkOfTheWebDeclaration(file_url).Utf8();
    original_str += original_contents;
    if (!doc.IsNull()) {
      WebString encoding = web_frame->GetDocument().Encoding();
      std::string htmlTag("<html>");
      std::string::size_type pos = original_str.find(htmlTag);
      ASSERT_NE(std::string::npos, pos);
      pos += htmlTag.length();
      std::string head_part("<head>");
      head_part +=
          WebFrameSerializer::GenerateMetaCharsetDeclaration(encoding).Utf8();
      head_part += "</head>";
      original_str.insert(pos, head_part);
    }
    ASSERT_EQ(original_str, serialized_contents_);
  }

  void SerializeHTMLDOMWithNonStandardEntitiesOnRenderer(const GURL& file_url) {
    // Get value of BODY's title attribute in DOM.
    WebLocalFrame* web_frame = FindSubFrameByURL(file_url);
    WebDocument doc = web_frame->GetDocument();
    ASSERT_TRUE(doc.IsHTMLDocument());
    WebElement body_element = doc.Body();
    // Unescaped string for "&percnt;&nsup;&sup1;&apos;".
    static const wchar_t parsed_value[] = {
      '%', 0x2285, 0x00b9, '\'', 0
    };
    WebString value = body_element.GetAttribute("title");
    WebString content = blink::WebFrameContentDumper::DumpWebViewAsText(
        web_frame->View(), 1024);
    ASSERT_TRUE(base::UTF16ToWide(value.Utf16()) == parsed_value);
    ASSERT_TRUE(base::UTF16ToWide(content.Utf16()) == parsed_value);

    // Do serialization.
    SerializeDomForURL(file_url);
    // Check the serialized string.
    ASSERT_TRUE(serialization_reported_end_of_data_);
    // Confirm that the serialized string has no non-standard HTML entities.
    ASSERT_EQ(std::string::npos, serialized_contents_.find("&percnt;"));
    ASSERT_EQ(std::string::npos, serialized_contents_.find("&nsup;"));
    ASSERT_EQ(std::string::npos, serialized_contents_.find("&sup1;"));
    ASSERT_EQ(std::string::npos, serialized_contents_.find("&apos;"));
  }

  void SerializeHTMLDOMWithBaseTagOnRenderer(const GURL& file_url,
                                             const GURL& path_dir_url) {
    // There are total 2 available base tags in this test file.
    const int kTotalBaseTagCountInTestFile = 2;

    // Since for this test, we assume there is no savable sub-resource links for
    // this test file, also all links are relative URLs in this test file, so we
    // need to check those relative URLs and make sure document has BASE tag.
    WebLocalFrame* web_frame = FindSubFrameByURL(file_url);
    ASSERT_TRUE(web_frame != nullptr);
    WebDocument doc = web_frame->GetDocument();
    ASSERT_TRUE(doc.IsHTMLDocument());
    // Go through all descent nodes.
    WebElementCollection all = doc.All();
    int original_base_tag_count = 0;
    for (WebElement element = all.FirstItem(); !element.IsNull();
         element = all.NextItem()) {
      if (element.HasHTMLTagName("base")) {
        original_base_tag_count++;
      } else {
        // Get link.
        WebString value = GetSubResourceLinkFromElement(element);
        if (value.IsNull() && element.HasHTMLTagName("a")) {
          value = element.GetAttribute("href");
          if (value.IsEmpty())
            value = WebString();
        }
        // Each link is relative link.
        if (!value.IsNull()) {
          GURL link(value.Utf8());
          ASSERT_TRUE(link.scheme().empty());
        }
      }
    }
    ASSERT_EQ(original_base_tag_count, kTotalBaseTagCountInTestFile);
    // Make sure in original document, the base URL is not equal with the
    // |path_dir_url|.
    GURL original_base_url(doc.BaseURL());
    ASSERT_NE(original_base_url, path_dir_url);

    // Do serialization.
    SerializeDomForURL(file_url);

    // Load the serialized contents.
    ASSERT_TRUE(serialization_reported_end_of_data_);
    LoadContents(serialized_contents_, file_url,
                 web_frame->GetDocument().Encoding());

    // Make sure all links are absolute URLs and doc there are some number of
    // BASE tags in serialized HTML data. Each of those BASE tags have same base
    // URL which is as same as URL of current test file.
    web_frame = GetMainFrame();
    ASSERT_TRUE(web_frame != nullptr);
    doc = web_frame->GetDocument();
    ASSERT_TRUE(doc.IsHTMLDocument());
    // Go through all descent nodes.
    all = doc.All();
    int new_base_tag_count = 0;
    for (WebNode node = all.FirstItem(); !node.IsNull();
         node = all.NextItem()) {
      if (!node.IsElementNode())
        continue;
      WebElement element = node.To<WebElement>();
      if (element.HasHTMLTagName("base")) {
        new_base_tag_count++;
      } else {
        // Get link.
        WebString value = GetSubResourceLinkFromElement(element);
        if (value.IsNull() && element.HasHTMLTagName("a")) {
          value = element.GetAttribute("href");
          if (value.IsEmpty())
            value = WebString();
        }
        // Each link is absolute link.
        if (!value.IsNull()) {
          GURL link(std::string(value.Utf8()));
          ASSERT_FALSE(link.scheme().empty());
        }
      }
    }
    // We have one more added BASE tag which is generated by JavaScript.
    ASSERT_EQ(new_base_tag_count, original_base_tag_count + 1);
    // Make sure in new document, the base URL is equal with the |path_dir_url|.
    GURL new_base_url(doc.BaseURL());
    ASSERT_EQ(new_base_url, path_dir_url);
  }

  void SerializeHTMLDOMWithEmptyHeadOnRenderer() {
    base::FilePath page_file_path = GetTestFilePath(
        "dom_serializer", "empty_head.htm");
    GURL file_url = net::FilePathToFileURL(page_file_path);
    ASSERT_TRUE(file_url.SchemeIsFile());

    // Load the test html content.
    static const char* const empty_head_contents =
      "<html><head></head><body>hello world</body></html>";
    LoadContents(empty_head_contents, file_url, WebString());

    // Make sure the head tag is empty.
    WebLocalFrame* web_frame = GetMainFrame();
    ASSERT_TRUE(web_frame != nullptr);
    WebDocument doc = web_frame->GetDocument();
    ASSERT_TRUE(doc.IsHTMLDocument());
    WebElement head_element = doc.Head();
    ASSERT_TRUE(!head_element.IsNull());
    ASSERT_TRUE(head_element.FirstChild().IsNull());

    // Do serialization.
    SerializeDomForURL(file_url);
    ASSERT_TRUE(serialization_reported_end_of_data_);

    // Reload serialized contents and make sure there is only one META tag.
    LoadContents(serialized_contents_, file_url,
                 web_frame->GetDocument().Encoding());
    web_frame = GetMainFrame();
    ASSERT_TRUE(web_frame != nullptr);
    doc = web_frame->GetDocument();
    ASSERT_TRUE(doc.IsHTMLDocument());
    head_element = doc.Head();
    ASSERT_TRUE(!head_element.IsNull());
    ASSERT_TRUE(!head_element.FirstChild().IsNull());
    ASSERT_TRUE(head_element.FirstChild().IsElementNode());
    ASSERT_TRUE(head_element.FirstChild().NextSibling().IsNull());
    WebMetaElement meta_element =
        head_element.FirstChild().To<WebMetaElement>();
    ASSERT_EQ(meta_element.ComputeEncoding(),
              web_frame->GetDocument().Encoding());

    // Check the body's first node is text node and its contents are
    // "hello world"
    WebElement body_element = doc.Body();
    ASSERT_TRUE(!body_element.IsNull());
    WebNode text_node = body_element.FirstChild();
    ASSERT_TRUE(text_node.IsTextNode());
    ASSERT_EQ("hello world", text_node.NodeValue());
  }

  void SubResourceForElementsInNonHTMLNamespaceOnRenderer(
      const GURL& file_url) {
    WebLocalFrame* web_frame = FindSubFrameByURL(file_url);
    ASSERT_TRUE(web_frame != nullptr);
    WebDocument doc = web_frame->GetDocument();
    WebNode lastNodeInBody = doc.Body().LastChild();
    ASSERT_TRUE(lastNodeInBody.IsElementNode());
    WebString uri =
        GetSubResourceLinkFromElement(lastNodeInBody.To<WebElement>());
    EXPECT_TRUE(uri.IsNull());
  }

 private:
  int32_t render_view_routing_id_;
  std::string serialized_contents_;
  bool serialization_reported_end_of_data_;
};

// If original contents have document type, the serialized contents also have
// document type.
// Disabled on OSX by ellyjones@ on 2015-05-18, see https://crbug.com/488495,
// on all platforms by tsergeant@ on 2016-03-10, see https://crbug.com/593575

IN_PROC_BROWSER_TEST_F(MAYBE_DomSerializerTests,
                       DISABLED_SerializeHTMLDOMWithDocType) {
  base::FilePath page_file_path =
      GetTestFilePath("dom_serializer", "youtube_1.htm");
  GURL file_url = net::FilePathToFileURL(page_file_path);
  ASSERT_TRUE(file_url.SchemeIsFile());
  // Load the test file.
  EXPECT_TRUE(NavigateToURL(shell(), file_url));

  PostTaskToInProcessRendererAndWait(base::BindOnce(
      &MAYBE_DomSerializerTests::SerializeHTMLDOMWithDocTypeOnRenderer,
      base::Unretained(this), file_url));
}

// If original contents do not have document type, the serialized contents
// also do not have document type.
IN_PROC_BROWSER_TEST_F(MAYBE_DomSerializerTests,
                       SerializeHTMLDOMWithoutDocType) {
  base::FilePath page_file_path =
      GetTestFilePath("dom_serializer", "youtube_2.htm");
  GURL file_url = net::FilePathToFileURL(page_file_path);
  ASSERT_TRUE(file_url.SchemeIsFile());
  // Load the test file.
  EXPECT_TRUE(NavigateToURL(shell(), file_url));

  PostTaskToInProcessRendererAndWait(base::BindOnce(
      &MAYBE_DomSerializerTests::SerializeHTMLDOMWithoutDocTypeOnRenderer,
      base::Unretained(this), file_url));
}

// Serialize XML document which has all 5 built-in entities. After
// finishing serialization, the serialized contents should be same
// with original XML document.
IN_PROC_BROWSER_TEST_F(MAYBE_DomSerializerTests,
                       SerializeXMLDocWithBuiltInEntities) {
  base::FilePath page_file_path =
      GetTestFilePath("dom_serializer", "note.html");
  base::FilePath xml_file_path = GetTestFilePath("dom_serializer", "note.xml");

  std::string original_contents;
  {
    // Read original contents for later comparison.
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::ReadFileToString(xml_file_path, &original_contents));
  }

  // Get file URL.
  GURL file_url = net::FilePathToFileURL(page_file_path);
  GURL xml_file_url = net::FilePathToFileURL(xml_file_path);
  ASSERT_TRUE(file_url.SchemeIsFile());

  // Load the test file.
  EXPECT_TRUE(NavigateToURL(shell(), file_url));

  PostTaskToInProcessRendererAndWait(base::BindOnce(
      &MAYBE_DomSerializerTests::SerializeXMLDocWithBuiltInEntitiesOnRenderer,
      base::Unretained(this), xml_file_url, original_contents));
}

// When serializing DOM, we add MOTW declaration before html tag.
IN_PROC_BROWSER_TEST_F(MAYBE_DomSerializerTests,
                       SerializeHTMLDOMWithAddingMOTW) {
  base::FilePath page_file_path =
      GetTestFilePath("dom_serializer", "youtube_2.htm");

  std::string original_contents;
  {
    // Read original contents for later comparison .
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::ReadFileToString(page_file_path, &original_contents));
  }

  // Get file URL.
  GURL file_url = net::FilePathToFileURL(page_file_path);
  ASSERT_TRUE(file_url.SchemeIsFile());

  // Load the test file.
  EXPECT_TRUE(NavigateToURL(shell(), file_url));

  PostTaskToInProcessRendererAndWait(base::BindOnce(
      &MAYBE_DomSerializerTests::SerializeHTMLDOMWithAddingMOTWOnRenderer,
      base::Unretained(this), file_url, original_contents, false));
}

// When serializing DOM, we add MOTW declaration before html tag.
IN_PROC_BROWSER_TEST_F(MAYBE_DomSerializerTests,
                       SerializeOffTheRecordHTMLDOMWithAddingMOTW) {
  base::FilePath page_file_path =
      GetTestFilePath("dom_serializer", "youtube_2.htm");

  std::string original_contents;
  {
    // Read original contents for later comparison .
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::ReadFileToString(page_file_path, &original_contents));
  }

  // Get file URL.
  GURL file_url = net::FilePathToFileURL(page_file_path);
  ASSERT_TRUE(file_url.SchemeIsFile());

  // Load the test file.
  EXPECT_TRUE(NavigateToURL(shell(), file_url));

  PostTaskToInProcessRendererAndWait(base::BindOnce(
      &MAYBE_DomSerializerTests::SerializeHTMLDOMWithAddingMOTWOnRenderer,
      base::Unretained(this), file_url, original_contents, true));
}

// When serializing DOM, we will add the META which have correct charset
// declaration as first child of HEAD element for resolving WebKit bug:
// http://bugs.webkit.org/show_bug.cgi?id=16621 even the original document
// does not have META charset declaration.
// Disabled on OSX by battre@ on 2015-05-21, see https://crbug.com/488495,
// on all platforms by tsergeant@ on 2016-03-10, see https://crbug.com/593575
IN_PROC_BROWSER_TEST_F(
    MAYBE_DomSerializerTests,
    DISABLED_SerializeHTMLDOMWithNoMetaCharsetInOriginalDoc) {
  base::FilePath page_file_path =
      GetTestFilePath("dom_serializer", "youtube_1.htm");
  // Get file URL.
  GURL file_url = net::FilePathToFileURL(page_file_path);
  ASSERT_TRUE(file_url.SchemeIsFile());
  // Load the test file.
  EXPECT_TRUE(NavigateToURL(shell(), file_url));

  PostTaskToInProcessRendererAndWait(base::BindOnce(
      &MAYBE_DomSerializerTests::
          SerializeHTMLDOMWithNoMetaCharsetInOriginalDocOnRenderer,
      base::Unretained(this), file_url));
}

// When serializing DOM, if the original document has multiple META charset
// declaration, we will add the META which have correct charset declaration
// as first child of HEAD element and remove all original META charset
// declarations.
// Disabled due to http://crbug.com/812904
IN_PROC_BROWSER_TEST_F(
    MAYBE_DomSerializerTests,
    DISABLED_SerializeHTMLDOMWithMultipleMetaCharsetInOriginalDoc) {
  base::FilePath page_file_path =
      GetTestFilePath("dom_serializer", "youtube_2.htm");
  // Get file URL.
  GURL file_url = net::FilePathToFileURL(page_file_path);
  ASSERT_TRUE(file_url.SchemeIsFile());
  // Load the test file.
  EXPECT_TRUE(NavigateToURL(shell(), file_url));

  PostTaskToInProcessRendererAndWait(base::BindOnce(
      &MAYBE_DomSerializerTests::
          SerializeHTMLDOMWithMultipleMetaCharsetInOriginalDocOnRenderer,
      base::Unretained(this), file_url));
}

// Test situation of html entities in text when serializing HTML DOM.
IN_PROC_BROWSER_TEST_F(MAYBE_DomSerializerTests,
                       SerializeHTMLDOMWithEntitiesInText) {
  // Need to spin up the renderer and also navigate to a file url so that the
  // renderer code doesn't attempt a fork when it sees a load to file scheme
  // from non-file scheme.
  EXPECT_TRUE(NavigateToURL(shell(), GetTestUrl(".", "simple_page.html")));

  PostTaskToInProcessRendererAndWait(base::BindOnce(
      &MAYBE_DomSerializerTests::SerializeHTMLDOMWithEntitiesInTextOnRenderer,
      base::Unretained(this)));
}

// Test situation of html entities in attribute value when serializing
// HTML DOM.
// This test started to fail at WebKit r65388. See http://crbug.com/52279.
IN_PROC_BROWSER_TEST_F(MAYBE_DomSerializerTests,
                       SerializeHTMLDOMWithEntitiesInAttributeValue) {
  // Need to spin up the renderer and also navigate to a file url so that the
  // renderer code doesn't attempt a fork when it sees a load to file scheme
  // from non-file scheme.
  EXPECT_TRUE(NavigateToURL(shell(), GetTestUrl(".", "simple_page.html")));

  PostTaskToInProcessRendererAndWait(
      base::BindOnce(&MAYBE_DomSerializerTests::
                         SerializeHTMLDOMWithEntitiesInAttributeValueOnRenderer,
                     base::Unretained(this)));
}

// Test situation of non-standard HTML entities when serializing HTML DOM.
// This test started to fail at WebKit r65351. See http://crbug.com/52279.
IN_PROC_BROWSER_TEST_F(MAYBE_DomSerializerTests,
                       SerializeHTMLDOMWithNonStandardEntities) {
  // Make a test file URL and load it.
  base::FilePath page_file_path = GetTestFilePath(
      "dom_serializer", "nonstandard_htmlentities.htm");
  GURL file_url = net::FilePathToFileURL(page_file_path);
  EXPECT_TRUE(NavigateToURL(shell(), file_url));

  PostTaskToInProcessRendererAndWait(
      base::BindOnce(&MAYBE_DomSerializerTests::
                         SerializeHTMLDOMWithNonStandardEntitiesOnRenderer,
                     base::Unretained(this), file_url));
}

// Test situation of BASE tag in original document when serializing HTML DOM.
// When serializing, we should comment the BASE tag, append a new BASE tag.
// rewrite all the savable URLs to relative local path, and change other URLs
// to absolute URLs.
IN_PROC_BROWSER_TEST_F(MAYBE_DomSerializerTests, SerializeHTMLDOMWithBaseTag) {
  base::FilePath page_file_path = GetTestFilePath(
      "dom_serializer", "html_doc_has_base_tag.htm");

  // Get page dir URL which is base URL of this file.
  base::FilePath dir_name = page_file_path.DirName();
  dir_name = dir_name.Append(
      base::FilePath::StringType(base::FilePath::kSeparators[0], 1));
  GURL path_dir_url = net::FilePathToFileURL(dir_name);

  // Get file URL.
  GURL file_url = net::FilePathToFileURL(page_file_path);
  ASSERT_TRUE(file_url.SchemeIsFile());
  // Load the test file.
  EXPECT_TRUE(NavigateToURL(shell(), file_url));

  PostTaskToInProcessRendererAndWait(base::BindOnce(
      &MAYBE_DomSerializerTests::SerializeHTMLDOMWithBaseTagOnRenderer,
      base::Unretained(this), file_url, path_dir_url));
}

// Serializing page which has an empty HEAD tag.
IN_PROC_BROWSER_TEST_F(MAYBE_DomSerializerTests,
                       SerializeHTMLDOMWithEmptyHead) {
  // Need to spin up the renderer and also navigate to a file url so that the
  // renderer code doesn't attempt a fork when it sees a load to file scheme
  // from non-file scheme.
  EXPECT_TRUE(NavigateToURL(shell(), GetTestUrl(".", "simple_page.html")));

  PostTaskToInProcessRendererAndWait(base::BindOnce(
      &MAYBE_DomSerializerTests::SerializeHTMLDOMWithEmptyHeadOnRenderer,
      base::Unretained(this)));
}

IN_PROC_BROWSER_TEST_F(MAYBE_DomSerializerTests,
                       SubResourceForElementsInNonHTMLNamespace) {
  base::FilePath page_file_path = GetTestFilePath(
      "dom_serializer", "non_html_namespace.htm");
  GURL file_url = net::FilePathToFileURL(page_file_path);
  EXPECT_TRUE(NavigateToURL(shell(), file_url));

  PostTaskToInProcessRendererAndWait(
      base::BindOnce(&MAYBE_DomSerializerTests::
                         SubResourceForElementsInNonHTMLNamespaceOnRenderer,
                     base::Unretained(this), file_url));
}

}  // namespace content
