// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <WebKit/WebKit.h>

#import "base/memory/ptr_util.h"
#import "base/memory/raw_ptr.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/scoped_feature_list.h"
#import "ios/web/annotations/annotations_java_script_feature.h"
#import "ios/web/common/annotations_utils.h"
#import "ios/web/common/features.h"
#import "ios/web/js_messaging/java_script_feature_manager.h"
#import "ios/web/js_messaging/web_frame_impl.h"
#import "ios/web/public/annotations/annotations_text_manager.h"
#import "ios/web/public/annotations/annotations_text_observer.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"

using base::test::ios::kWaitForActionTimeout;
using base::test::ios::kWaitForJSCompletionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace web {

namespace {
// This is for cases where no message should be sent back from Js.
constexpr base::TimeDelta kWaitForJsNotReturnTimeout = base::Milliseconds(500);

const char kTestScriptName[] = "annotations_test";
const char kNoViewportScriptName[] = "annotations";
const char kViewportScriptName[] = "text_main";

// Feature to include test ts code only.
class AnnotationsTestJavaScriptFeature : public JavaScriptFeature {
 public:
  AnnotationsTestJavaScriptFeature(std::string script_name)
      : JavaScriptFeature(
            ContentWorld::kIsolatedWorld,
            {FeatureScript::CreateWithFilename(
                 kTestScriptName,
                 FeatureScript::InjectionTime::kDocumentStart,
                 FeatureScript::TargetFrames::kMainFrame,
                 FeatureScript::ReinjectionBehavior::kInjectOncePerWindow),
             FeatureScript::CreateWithFilename(
                 script_name,
                 FeatureScript::InjectionTime::kDocumentStart,
                 FeatureScript::TargetFrames::kMainFrame,
                 FeatureScript::ReinjectionBehavior::kInjectOncePerWindow)}) {}

  AnnotationsTestJavaScriptFeature(const AnnotationsTestJavaScriptFeature&) =
      delete;
  AnnotationsTestJavaScriptFeature& operator=(
      const AnnotationsTestJavaScriptFeature&) = delete;
};

// Class used to observe AnnotationTextManager interactions with an observer.
class TestAnnotationTextObserver : public AnnotationsTextObserver {
 public:
  TestAnnotationTextObserver()
      : successes_(0), annotations_(0), clicks_(0), decoration_calls_(0) {}

  TestAnnotationTextObserver(const TestAnnotationTextObserver&) = delete;
  TestAnnotationTextObserver& operator=(const TestAnnotationTextObserver&) =
      delete;

  void OnTextExtracted(WebState* web_state,
                       const std::string& text,
                       int seq_id,
                       const base::Value::Dict& metadata) override {
    extracted_text_ = text;
    EXPECT_GE(seq_id, 1);
    seq_id_ = seq_id;
    metadata_ = metadata.Clone();
  }

  void OnDecorated(WebState* web_state,
                   int annotations,
                   int successes,
                   int failures,
                   const base::Value::List& cancelled) override {
    decoration_calls_++;
    annotations_ = annotations;
    successes_ = successes;
    failures_ = failures;
  }

  void OnClick(WebState* web_state,
               const std::string& text,
               CGRect rect,
               const std::string& data) override {
    clicks_++;
    click_data_ = data;
  }

  void Reset() { seq_id_ = 0; }

  const std::string& extracted_text() const { return extracted_text_; }
  int successes() const { return successes_; }
  int failures() const { return failures_; }
  int annotations() const { return annotations_; }
  int clicks() const { return clicks_; }
  int seq_id() const { return seq_id_; }
  const base::Value::Dict& metadata() const { return metadata_; }
  const std::string& click_data() const { return click_data_; }
  void SetAnnotations(int count) { annotations_ = count; }
  int decoration_calls() const { return decoration_calls_; }

 private:
  std::string extracted_text_, click_data_;
  int successes_, failures_, annotations_, clicks_, seq_id_, decoration_calls_;
  base::Value::Dict metadata_;
};

}  // namespace

// Test fixture for WebStateDelegate::FaviconUrlUpdated and integration tests.
class AnnotationTextManagerTest : public web::WebTestWithWebState {
 public:
  AnnotationTextManagerTest() = default;

  AnnotationTextManagerTest(const AnnotationTextManagerTest&) = delete;
  AnnotationTextManagerTest& operator=(const AnnotationTextManagerTest&) =
      delete;

 protected:
  void SetUp() override {
    WebTestWithWebState::SetUp();

    AnnotationsTextManager::CreateForWebState(web_state());
    auto* manager = AnnotationsTextManager::FromWebState(web_state());
    manager->AddObserver(&observer_);
    manager->SetSupportedTypes(NSTextCheckingAllTypes);

    WKWebViewConfigurationProvider& configuration_provider =
        WKWebViewConfigurationProvider::FromBrowserState(GetBrowserState());
    // Force the creation of the content worlds.
    configuration_provider.GetWebViewConfiguration();

    content_world_ =
        JavaScriptFeatureManager::FromBrowserState(GetBrowserState())
            ->GetContentWorldForFeature(
                AnnotationsJavaScriptFeature::GetInstance());
    js_test_feature_ =
        std::make_unique<AnnotationsTestJavaScriptFeature>(GetScriptName());
    // Inject ts test helpers functions.
    content_world_->AddFeature(js_test_feature_.get());
  }

  void TearDown() override {
    auto* manager = AnnotationsTextManager::FromWebState(web_state());
    manager->RemoveObserver(&observer_);
    WebTestWithWebState::TearDown();
  }

  virtual std::string GetScriptName() { return ""; }

  bool WaitForWebFramesCount(unsigned long web_frames_count) {
    return WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
      return AllWebFrames().size() == web_frames_count;
    });
  }

  // Returns all web frames for `web_state()`.
  std::set<WebFrameImpl*> AllWebFrames() {
    std::set<WebFrameImpl*> frames;
    for (WebFrame* frame :
         web_state()->GetPageWorldWebFramesManager()->GetAllWebFrames()) {
      frames.insert(static_cast<WebFrameImpl*>(frame));
    }
    return frames;
  }

  // Returns main frame for `web_state_`.
  WebFrameInternal* MainWebFrame() {
    WebFrame* main_frame =
        web_state()->GetPageWorldWebFramesManager()->GetMainWebFrame();
    return main_frame->GetWebFrameInternal();
  }

  // Loads given `html` and waits until text is extracted.
  virtual void LoadHtmlAndExtractText(const std::string& html) {
    int seq_id = observer()->seq_id();
    ASSERT_TRUE(LoadHtml(html));
    ASSERT_TRUE(WaitForWebFramesCount(1));

    // Wait for text extracted, background parsing and decoration.
    // Make timeout 4 times the regular action timeout to reduce flakiness.
    EXPECT_TRUE(WaitUntilConditionOrTimeout(4 * kWaitForActionTimeout, ^{
      return observer()->seq_id() > seq_id;
    }));
  }

  // Creates and applies annotations based on `source` text and all matching
  // `items`. `items` is a dictionary when the key is the annotation type to
  // apply to the its values.
  void CreateAndApplyAnnotationsWithTypes(
      NSString* source,
      NSDictionary<NSString*, NSArray<NSString*>*>* items,
      int seq_id) {
    int decoration_calls = observer()->decoration_calls();
    // Create annotation.
    base::Value::List annotations;
    for (NSString* type in items) {
      for (NSString* item in items[type]) {
        NSRange range = [source rangeOfString:item];
        web::TextAnnotation annotation =
            web::ConvertMatchToAnnotation(source, range, nil, type);
        annotation.first.Set(
            "data", base::SysNSStringToUTF8(
                        [NSString stringWithFormat:@"%@-%@", type, item]));
        annotations.Append(base::Value(std::move(annotation.first)));
      }
    }
    auto* manager = AnnotationsTextManager::FromWebState(web_state());
    base::Value value = base::Value(std::move(annotations));
    manager->DecorateAnnotations(web_state(), value, seq_id);

    EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^{
      return observer()->decoration_calls() > decoration_calls &&
             observer()->annotations() > 0;
    }));
  }

  // Creates and applies annotations based on `source` text and all matching
  // `items` with type "type".
  void CreateAndApplyAnnotations(NSString* source,
                                 NSArray<NSString*>* items,
                                 int seq_id) {
    CreateAndApplyAnnotationsWithTypes(source, @{@"type" : items}, seq_id);
  }

  // Verifies the now state of html text and tags of the document. Tags have no
  // properties.
  void CheckHtml(const std::string& html) {
    const base::TimeDelta kCallJavascriptFunctionTimeout =
        kWaitForJSCompletionTimeout;
    __block bool message_received = false;
    base::Value::List params;
    params.Append(1000);
    MainWebFrame()->CallJavaScriptFunctionInContentWorld(
        "annotationsTest.getPageTaggedText", params, content_world_,
        base::BindOnce(^(const base::Value* result) {
          ASSERT_TRUE(result);
          ASSERT_TRUE(result->is_string());
          EXPECT_EQ(html, result->GetString());
          message_received = true;
        }),
        kCallJavascriptFunctionTimeout);
    ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
      return message_received;
    }));
  }

  // Simulates clicking on annotation at given `index`.
  void ClickAnnotation(int index, bool viewport = false) {
    const base::TimeDelta kCallJavascriptFunctionTimeout =
        kWaitForJSCompletionTimeout;
    __block bool message_received = false;
    base::Value::List params;
    params.Append(index);
    params.Append(viewport);
    MainWebFrame()->CallJavaScriptFunctionInContentWorld(
        "annotationsTest.clickAnnotation", params, content_world_,
        base::BindOnce(^(const base::Value* result) {
          ASSERT_TRUE(result);
          ASSERT_TRUE(result->is_bool());
          EXPECT_TRUE(result->GetBool());
          message_received = true;
        }),
        kCallJavascriptFunctionTimeout);
    ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
      return message_received;
    }));
  }

  // Updates count of annotations in observer.
  void CountAnnotation() {
    const base::TimeDelta kCallJavascriptFunctionTimeout =
        kWaitForJSCompletionTimeout;
    __block bool message_received = false;
    base::Value::List params;
    MainWebFrame()->CallJavaScriptFunctionInContentWorld(
        "annotationsTest.countAnnotations", params, content_world_,
        base::BindOnce(^(const base::Value* result) {
          ASSERT_TRUE(result);
          ASSERT_TRUE(result->is_double());
          observer_.SetAnnotations(result->GetDouble());
          message_received = true;
        }),
        kCallJavascriptFunctionTimeout);
    ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
      return message_received;
    }));
  }

  TestAnnotationTextObserver* observer() { return &observer_; }

  base::test::ScopedFeatureList feature_;
  raw_ptr<JavaScriptContentWorld> content_world_;
  TestAnnotationTextObserver observer_;
  std::unique_ptr<AnnotationsTestJavaScriptFeature> js_test_feature_;
};

class AnnotationTextManagerNoViewportTest : public AnnotationTextManagerTest {
 public:
  AnnotationTextManagerNoViewportTest() = default;

  AnnotationTextManagerNoViewportTest(
      const AnnotationTextManagerNoViewportTest&) = delete;
  AnnotationTextManagerNoViewportTest& operator=(
      const AnnotationTextManagerNoViewportTest&) = delete;

 protected:
  void SetUp() override {
    feature_.InitAndDisableFeature(features::kEnableViewportIntents);
    AnnotationTextManagerTest::SetUp();
  }

  std::string GetScriptName() override { return kNoViewportScriptName; }
};

class AnnotationTextManagerViewportTest : public AnnotationTextManagerTest {
 public:
  AnnotationTextManagerViewportTest() = default;

  AnnotationTextManagerViewportTest(const AnnotationTextManagerViewportTest&) =
      delete;
  AnnotationTextManagerViewportTest& operator=(
      const AnnotationTextManagerViewportTest&) = delete;

 protected:
  void SetUp() override {
    feature_.InitAndEnableFeature(features::kEnableViewportIntents);
    AnnotationTextManagerTest::SetUp();
  }

  std::string GetScriptName() override { return kViewportScriptName; }

  void LoadHtmlAndExtractText(const std::string& html) override {
    observer()->Reset();
    AnnotationTextManagerTest::LoadHtmlAndExtractText(html);
  }
};

// Tests page text extraction.
// Covers: PageLoaded, OnTextExtracted, StartExtractingText.
TEST_F(AnnotationTextManagerNoViewportTest, ExtractText) {
  LoadHtmlAndExtractText("<html><body>"
                         "<p>You'll find it on</p>"
                         "<p>Castro Street, <span>Mountain View</span>, CA</p>"
                         "<p>Enjoy</p>"
                         "</body></html>");

  EXPECT_EQ("You'll find it on"
            "\nCastro Street, Mountain View, CA"
            "\nEnjoy",
            observer()->extracted_text());
}

// Tests page text extraction.
// Covers: PageLoaded, OnTextExtracted, StartExtractingText.
TEST_F(AnnotationTextManagerViewportTest, ExtractText) {
  LoadHtmlAndExtractText("<html><body>"
                         "<p>You'll find it on</p>"
                         "<p>Castro Street, <span>Mountain View</span>, CA</p>"
                         "<p>Enjoy</p>"
                         "</body></html>");

  EXPECT_EQ("You'll find it on "
            "Castro Street, Mountain View, CA "
            "Enjoy ",
            observer()->extracted_text());
}

// Tests page text extraction with different type of tags.
TEST_F(AnnotationTextManagerViewportTest, ExtractTextTags) {
  LoadHtmlAndExtractText("<html><body>"
                         "<div>abc<div>def<span>ghi</span><em>jkl</em>mno</div>"
                         "pqr</div>"
                         "</body></html>");

  EXPECT_EQ("abc defghijklmno pqr ", observer()->extracted_text());
}

// Tests no page text extraction if there is no supported type.
TEST_F(AnnotationTextManagerNoViewportTest, ExtractNoText) {
  auto* manager = AnnotationsTextManager::FromWebState(web_state());
  manager->SetSupportedTypes(0);

  int seq_id = observer()->seq_id();

  ASSERT_TRUE(LoadHtml("<html><body>"
                       "<p>You'll find it on</p>"
                       "<p>Castro Street, <span>Mountain View</span>, CA</p>"
                       "<p>Enjoy</p>"
                       "</body></html>"));
  ASSERT_TRUE(WaitForWebFramesCount(1));
  EXPECT_FALSE(WaitUntilConditionOrTimeout(kWaitForJsNotReturnTimeout, ^{
    return observer()->seq_id() > seq_id;
  }));
  EXPECT_EQ("", observer()->extracted_text());
}

// Tests no page text extraction if there is no supported type.
TEST_F(AnnotationTextManagerViewportTest, ExtractNoText) {
  auto* manager = AnnotationsTextManager::FromWebState(web_state());
  manager->SetSupportedTypes(0);

  int seq_id = observer()->seq_id();

  ASSERT_TRUE(LoadHtml("<html><body>"
                       "<p>You'll find it on</p>"
                       "<p>Castro Street, <span>Mountain View</span>, CA</p>"
                       "<p>Enjoy</p>"
                       "</body></html>"));
  ASSERT_TRUE(WaitForWebFramesCount(1));
  EXPECT_FALSE(WaitUntilConditionOrTimeout(kWaitForJsNotReturnTimeout, ^{
    return observer()->seq_id() > seq_id;
  }));
  EXPECT_EQ("", observer()->extracted_text());
}

TEST_F(AnnotationTextManagerNoViewportTest, CheckMetadata) {
  LoadHtmlAndExtractText("<html lang=\"fr\">"
                         "<head>"
                         "<meta http-equiv=\"content-language\" content=\"fr\">"
                         "<meta name=\"chrome\" content=\"nointentdetection\"/>"
                         "<meta name=\"google\" content=\"notranslate\"/>"
                         "</head>"
                         "<body>"
                         "<p>You'll find it on</p>"
                         "<p>Castro Street, <span>Mountain View</span>, CA</p>"
                         "<p>Enjoy</p>"
                         "</body></html>");
  std::string fr = "fr";
  EXPECT_TRUE(observer()->metadata().FindBool("hasNoIntentDetection").value());
  EXPECT_TRUE(observer()->metadata().FindBool("hasNoTranslate").value());
  EXPECT_EQ(fr, *observer()->metadata().FindString("htmlLang"));
  EXPECT_EQ(fr, *observer()->metadata().FindString("httpContentLanguage"));
}

TEST_F(AnnotationTextManagerViewportTest, CheckMetadata) {
  int seq_id = observer()->seq_id();

  ASSERT_TRUE(LoadHtml("<html lang=\"fr\">"
                       "<head>"
                       "<meta http-equiv=\"content-language\" content=\"fr\">"
                       "<meta name=\"chrome\" content=\"nointentdetection\"/>"
                       "<meta name=\"google\" content=\"notranslate\"/>"
                       "</head>"
                       "<body>"
                       "<p>You'll find it on</p>"
                       "<p>Castro Street, <span>Mountain View</span>, CA</p>"
                       "<p>Enjoy</p>"
                       "</body></html>"));
  ASSERT_TRUE(WaitForWebFramesCount(1));
  EXPECT_FALSE(WaitUntilConditionOrTimeout(kWaitForJsNotReturnTimeout, ^{
    return observer()->seq_id() > seq_id;
  }));
  EXPECT_EQ("", observer()->extracted_text());
}

TEST_F(AnnotationTextManagerNoViewportTest, CheckWkMetadata) {
  LoadHtmlAndExtractText(
      "<html lang=\"fr\">"
      "<head>"
      "<meta name=\"format-detection\" content=\"telephone=no\"/>"
      "</head>"
      "<body>"
      "<p>You'll find it on</p>"
      "<p>Castro Street, <span>Mountain View</span>, CA</p>"
      "<p>Enjoy</p>"
      "</body></html>");
  EXPECT_TRUE(observer()->metadata().FindBool("wkNoTelephone").value());
}

TEST_F(AnnotationTextManagerViewportTest, CheckWkMetadata) {
  int seq_id = observer()->seq_id();

  LoadHtmlAndExtractText(
      "<html lang=\"fr\">"
      "<head>"
      "<meta name=\"format-detection\" content=\"telephone=no\"/>"
      "</head>"
      "<body>"
      "<p>You'll find it on</p>"
      "<p>Castro Street, <span>Mountain View</span>, CA</p>"
      "<p>Enjoy</p>"
      "</body></html>");
  ASSERT_TRUE(WaitForWebFramesCount(1));
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^{
    return observer()->seq_id() > seq_id;
  }));
  EXPECT_TRUE(observer()->metadata().FindBool("wkNoTelephone").value());
}

TEST_F(AnnotationTextManagerNoViewportTest, CheckNoMetadata) {
  LoadHtmlAndExtractText("<html>"
                         "<head>"
                         "</head>"
                         "<body>"
                         "<p>You'll find it on</p>"
                         "<p>Castro Street, <span>Mountain View</span>, CA</p>"
                         "<p>Enjoy</p>"
                         "</body></html>");
  std::string empty = "";
  EXPECT_FALSE(observer()->metadata().FindBool("hasNoIntentDetection").value());
  EXPECT_FALSE(observer()->metadata().FindBool("hasNoTranslate").value());
  EXPECT_EQ(empty, *observer()->metadata().FindString("htmlLang"));
  EXPECT_EQ(empty, *observer()->metadata().FindString("httpContentLanguage"));
}

TEST_F(AnnotationTextManagerViewportTest, CheckNoMetadata) {
  LoadHtmlAndExtractText("<html>"
                         "<head>"
                         "</head>"
                         "<body>"
                         "<p>You'll find it on</p>"
                         "<p>Castro Street, <span>Mountain View</span>, CA</p>"
                         "<p>Enjoy</p>"
                         "</body></html>");
  std::string empty = "";
  EXPECT_FALSE(observer()->metadata().FindBool("hasNoIntentDetection"));
  EXPECT_FALSE(observer()->metadata().FindBool("hasNoTranslate"));
  EXPECT_EQ(empty, *observer()->metadata().FindString("htmlLang"));
  EXPECT_EQ(empty, *observer()->metadata().FindString("httpContentLanguage"));
}

// Tests page decoration when page doesn't change.
// Covers: DecorateAnnotations, ConvertMatchToAnnotation.
TEST_F(AnnotationTextManagerNoViewportTest, DecorateText) {
  LoadHtmlAndExtractText("<html><body>"
                         "<p>text</p>"
                         "<p>annotation</p>"
                         "<p>text</p>"
                         "</body></html>");

  std::string text = "text"
                     "\nannotation"
                     "\ntext";
  EXPECT_EQ(text, observer()->extracted_text());

  // Create annotation.
  NSString* source = base::SysUTF8ToNSString(text);
  CreateAndApplyAnnotations(source, @[ @"annotation" ], observer() -> seq_id());

  EXPECT_EQ(observer()->successes(), 1);
  EXPECT_EQ(observer()->annotations(), 1);

  // Check the resulting html is annotating at the right place.
  CheckHtml("<html><body>"
            "<p>text</p>"
            "<p><chrome_annotation>annotation</chrome_annotation></p>"
            "<p>text</p>"
            "</body></html>");
}

// Tests page decoration when page doesn't change.
// Covers: DecorateAnnotations, ConvertMatchToAnnotation.
TEST_F(AnnotationTextManagerViewportTest, DecorateText) {
  LoadHtmlAndExtractText("<html><body>"
                         "<p>text</p>"
                         "<p>annotation</p>"
                         "<p>text</p>"
                         "</body></html>");

  std::string text = "text "
                     "annotation "
                     "text ";
  EXPECT_EQ(text, observer()->extracted_text());

  // Create annotation.
  NSString* source = base::SysUTF8ToNSString(text);
  CreateAndApplyAnnotations(source, @[ @"annotation" ], observer() -> seq_id());

  EXPECT_EQ(observer()->successes(), 1);
  EXPECT_EQ(observer()->annotations(), 1);

  // Check the resulting html is annotating at the right place.
  CheckHtml("<html><body>"
            "<p>text</p>"
            "<p><chrome_annotation>annotation</chrome_annotation></p>"
            "<p>text</p>"
            "</body></html>");
}

// Tests the if the original node is updated, the annotation is restored.
TEST_F(AnnotationTextManagerViewportTest, UpdateDecoratedText) {
  LoadHtmlAndExtractText("<html><body>"
                         "<p>text</p>"
                         "<p id='annotated'>annotation</p>"
                         "<p>text</p>"
                         "</body></html>");

  // Simulate page accessing the DOM, so out of the content world.
  ExecuteJavaScript(@"var annotated_element = "
                    @"document.getElementById('annotated').childNodes[0];");

  std::string text = "text "
                     "annotation "
                     "text ";
  EXPECT_EQ(text, observer()->extracted_text());

  // Create annotation.
  NSString* source = base::SysUTF8ToNSString(text);
  CreateAndApplyAnnotations(source, @[ @"annotation" ], observer() -> seq_id());

  EXPECT_EQ(observer()->successes(), 1);
  EXPECT_EQ(observer()->annotations(), 1);

  // Check the resulting html is annotating at the right place.
  CheckHtml("<html><body>"
            "<p>text</p>"
            "<p><chrome_annotation>annotation</chrome_annotation></p>"
            "<p>text</p>"
            "</body></html>");

  // Simulate page accessing the DOM, so out of the content world.
  ExecuteJavaScript(@"annotated_element.textContent = 'ANNOTATION';");
  // Check the annotation disappeared so that the change is visible.
  CheckHtml("<html><body>"
            "<p>text</p>"
            "<p>ANNOTATION</p>"
            "<p>text</p>"
            "</body></html>");
}

// Tests on no-decoration tags.
TEST_F(AnnotationTextManagerNoViewportTest, NoDecorateText) {
  LoadHtmlAndExtractText("<html><body>"
                         "<p>text</p>"
                         "<a>annotation1</a>"
                         "<input type=\"radio\">"
                         "<label>annotation2</label>"
                         "<p>text</p>"
                         "</body></html>");

  std::string text = "text"
                     "\ntext";
  EXPECT_EQ(text, observer()->extracted_text());
}

// Tests on no-decoration tags.
TEST_F(AnnotationTextManagerViewportTest, NoDecorateText) {
  LoadHtmlAndExtractText("<html><body>"
                         "<p>text</p>"
                         "<a>annotation1</a>"
                         "<input type=\"radio\">"
                         "<label>annotation2</label>"
                         "<p>text</p>"
                         "</body></html>");

  std::string text = "text  ‡  "
                     "text ";
  EXPECT_EQ(text, observer()->extracted_text());
}

// Tests different annotation cases, including tags boundaries.
// Covers: RemoveDecorations
TEST_F(AnnotationTextManagerNoViewportTest, DecorateTextCrossingElements) {
  std::string html = "<html><body>"
                     "<p>abc</p>"
                     "<p>def</p>"
                     "<p>ghi</p>"
                     "<p>jkl</p>"
                     "<p>mno</p>"
                     "</body></html>";
  LoadHtmlAndExtractText(html);

  NSString* source = base::SysUTF8ToNSString(observer()->extracted_text());
  CreateAndApplyAnnotations(source, @[ @"a", @"c\nd", @"f\nghi\nj", @"l\nmno" ],
                            observer() -> seq_id());

  // Check the resulting html is annotating at the right place.
  CheckHtml("<html><body>"
            "<p><chrome_annotation>a</chrome_annotation>b<chrome_annotation>c</"
            "chrome_annotation></p>"
            "<p><chrome_annotation>d</chrome_annotation>e<chrome_annotation>f</"
            "chrome_annotation></p>"
            "<p><chrome_annotation>ghi</chrome_annotation></p>"
            "<p><chrome_annotation>j</chrome_annotation>k<chrome_annotation>l</"
            "chrome_annotation></p>"
            "<p><chrome_annotation>mno</chrome_annotation></p>"
            "</body></html>");

  // Make sure it's back to the original.
  auto* manager = AnnotationsTextManager::FromWebState(web_state());
  manager->RemoveDecorations();
  CheckHtml(html);
}

// Tests decoration across elements for addresses.
// Covers: RemoveDecorations
TEST_F(AnnotationTextManagerViewportTest, DecorateTextCrossingElements) {
  std::string html = "<html><body>"
                     "<p>abc</p>"
                     "<p>def</p>"
                     "<p>ghi</p>"
                     "<p>jkl</p>"
                     "<p>mno</p>"
                     "</body></html>";
  LoadHtmlAndExtractText(html);

  NSString* source = base::SysUTF8ToNSString(observer()->extracted_text());
  CreateAndApplyAnnotationsWithTypes(
      source,
      @{@"address" : @[ @"a", @"c d", @"f ghi j", @"l mno " ]},
      observer()->seq_id());

  // Check the resulting html is annotating at the right place.
  CheckHtml("<html><body>"
            "<p><chrome_annotation>a</chrome_annotation>b<chrome_annotation>c</"
            "chrome_annotation></p>"
            "<p><chrome_annotation>d</chrome_annotation>e<chrome_annotation>f</"
            "chrome_annotation></p>"
            "<p><chrome_annotation>ghi</chrome_annotation></p>"
            "<p><chrome_annotation>j</chrome_annotation>k<chrome_annotation>l</"
            "chrome_annotation></p>"
            "<p><chrome_annotation>mno</chrome_annotation></p>"
            "</body></html>");

  // Make sure it's back to the original.
  auto* manager = AnnotationsTextManager::FromWebState(web_state());
  manager->RemoveDecorations();
  CheckHtml(html);
}

// Tests no decoration accross elements for other types.
// Covers: RemoveDecorations
TEST_F(AnnotationTextManagerViewportTest, DontDecorateTextCrossingElements) {
  std::string html = "<html><body>"
                     "<p>abc</p>"
                     "<p>def</p>"
                     "<p>ghi</p>"
                     "<p>jkl</p>"
                     "<p>mno</p>"
                     "</body></html>";
  LoadHtmlAndExtractText(html);

  NSString* source = base::SysUTF8ToNSString(observer()->extracted_text());
  CreateAndApplyAnnotationsWithTypes(
      source,
      @{@"phone" : @[ @"a", @"c d", @"f ghi j", @"l mno " ]},
      observer()->seq_id());

  // Check the resulting html is annotating at the right place.
  CheckHtml("<html><body>"
            "<p><chrome_annotation>a</chrome_annotation>"
            "bc</p><p>def</p><p>ghi</p><p>jkl</p><p>mno</p></body></html>");

  // Make sure it's back to the original.
  auto* manager = AnnotationsTextManager::FromWebState(web_state());
  manager->RemoveDecorations();
  CheckHtml(html);
}

// Tests annotation cases with line breaks, including tags boundaries.
// Covers: DecorateAnnotations, RemoveDecorations
TEST_F(AnnotationTextManagerNoViewportTest, DecorateTextBreakElements) {
  std::string html = "<html><body>"
                     "<p>abc<br>\ndef</p>"
                     "</body></html>";
  LoadHtmlAndExtractText(html);
  CheckHtml(html);

  NSString* source = base::SysUTF8ToNSString(observer()->extracted_text());
  CreateAndApplyAnnotations(source, @[ @"abc\n\ndef" ], observer() -> seq_id());

  // Check the resulting html is annotating at the right place.
  CheckHtml("<html><body>"
            "<p><chrome_annotation>abc</chrome_annotation><br>"
            "<chrome_annotation>\ndef</chrome_annotation></p>"
            "</body></html>");

  // Make sure it's back to the original.
  auto* manager = AnnotationsTextManager::FromWebState(web_state());
  manager->RemoveDecorations();
  CheckHtml(html);
}

// Tests annotation cases with line breaks, including tags boundaries.
// Covers: DecorateAnnotations, RemoveDecorations
TEST_F(AnnotationTextManagerViewportTest, DecorateTextBreakElements) {
  std::string html = "<html><body>"
                     "<p>abc<br>\ndef</p>"
                     "</body></html>";
  LoadHtmlAndExtractText(html);
  CheckHtml(html);

  NSString* source = base::SysUTF8ToNSString(observer()->extracted_text());
  // ` ‡ ` is used as a section break to avoid cross section annotations.
  // Only address is allowed across elements.
  CreateAndApplyAnnotationsWithTypes(
      source,
      @{@"address" : @[ @"abc ‡ \ndef" ]}, observer()->seq_id());

  // Check the resulting html is annotating at the right place.
  CheckHtml("<html><body>"
            "<p><chrome_annotation>abc</chrome_annotation><br>"
            "<chrome_annotation>\ndef</chrome_annotation></p>"
            "</body></html>");

  // Make sure it's back to the original.
  auto* manager = AnnotationsTextManager::FromWebState(web_state());
  manager->RemoveDecorations();
  CheckHtml(html);
}

// Tests on click handler.
// Covers: OnClick.
TEST_F(AnnotationTextManagerNoViewportTest, ClickAnnotation) {
  LoadHtmlAndExtractText("<html><body>"
                         "<p>text</p>"
                         "<p>annotation</p>"
                         "<p>text</p>"
                         "</body></html>");
  NSString* source = base::SysUTF8ToNSString(observer()->extracted_text());
  CreateAndApplyAnnotations(source, @[ @"annotation" ], observer() -> seq_id());
  ClickAnnotation(0);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return observer()->clicks() == 1;
  }));
}

// Tests on click handler.
// Covers: OnClick.
TEST_F(AnnotationTextManagerViewportTest, ClickAnnotation) {
  LoadHtmlAndExtractText("<html><body>"
                         "<p>text</p>"
                         "<p>annotation</p>"
                         "<p>text</p>"
                         "</body></html>");
  NSString* source = base::SysUTF8ToNSString(observer()->extracted_text());
  CreateAndApplyAnnotations(source, @[ @"annotation" ], observer() -> seq_id());
  ClickAnnotation(0, true);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return observer()->clicks() == 1;
  }));
}

// Tests removing annotation of one type
TEST_F(AnnotationTextManagerNoViewportTest, RemoveDecorationTypeTest) {
  std::string html = "<html><body>"
                     "<p>abc def</p>"
                     "<p>zzzzz ghi zzzzz</p>"
                     "<p>zzzzz klm zzzzz</p>"
                     "</body></html>";
  LoadHtmlAndExtractText(html);
  CheckHtml(html);
  auto* manager = AnnotationsTextManager::FromWebState(web_state());

  NSString* source = base::SysUTF8ToNSString(observer()->extracted_text());

  CreateAndApplyAnnotationsWithTypes(
      source,
      @{@"type1" : @[ @"abc", @"ghi" ],
        @"type2" : @[ @"def", @"klm" ]},
      observer()->seq_id());

  // Check the resulting html is annotating at the right place.
  CheckHtml("<html><body>"
            "<p><chrome_annotation>abc</chrome_annotation> "
            "<chrome_annotation>def</chrome_annotation></p>"
            "<p>zzzzz <chrome_annotation>ghi</chrome_annotation> zzzzz</p>"
            "<p>zzzzz <chrome_annotation>klm</chrome_annotation> zzzzz</p>"
            "</body></html>");

  CountAnnotation();
  ASSERT_EQ(observer()->annotations(), 4);

  ClickAnnotation(0);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return observer()->clicks() == 1;
  }));

  ClickAnnotation(1);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return observer()->clicks() == 2;
  }));

  ClickAnnotation(2);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return observer()->clicks() == 3;
  }));

  ClickAnnotation(3);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return observer()->clicks() == 4;
  }));

  manager->RemoveDecorationsWithType("type1");
  // Check the resulting html is annotating at the right place.
  CheckHtml("<html><body>"
            "<p>abc <chrome_annotation>def</chrome_annotation></p>"
            "<p>zzzzz ghi zzzzz</p>"
            "<p>zzzzz <chrome_annotation>klm</chrome_annotation> zzzzz</p>"
            "</body></html>");

  CountAnnotation();
  ASSERT_EQ(observer()->annotations(), 2);

  ClickAnnotation(0);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return observer()->clicks() == 5;
  }));

  ClickAnnotation(1);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return observer()->clicks() == 6;
  }));

  // Make sure it's back to the original.
  manager->RemoveDecorations();
  CheckHtml(html);
}

// Tests removing annotation of one type
TEST_F(AnnotationTextManagerViewportTest, RemoveDecorationTypeTest) {
  std::string html = "<html><body>"
                     "<p>abc def</p>"
                     "<p>zzzzz ghi zzzzz</p>"
                     "<p>zzzzz klm zzzzz</p>"
                     "</body></html>";
  LoadHtmlAndExtractText(html);
  CheckHtml(html);
  auto* manager = AnnotationsTextManager::FromWebState(web_state());

  NSString* source = base::SysUTF8ToNSString(observer()->extracted_text());

  CreateAndApplyAnnotationsWithTypes(
      source,
      @{@"type1" : @[ @"abc", @"ghi" ],
        @"type2" : @[ @"def", @"klm" ]},
      observer()->seq_id());

  // Check the resulting html is annotating at the right place.
  CheckHtml("<html><body>"
            "<p><chrome_annotation>abc</chrome_annotation><span> </span>"
            "<chrome_annotation>def</chrome_annotation></p>"
            "<p>zzzzz<span> </span><chrome_annotation>ghi</chrome_annotation>"
            " zzzzz</p>"
            "<p>zzzzz<span> </span><chrome_annotation>klm</chrome_annotation>"
            " zzzzz</p>"
            "</body></html>");

  CountAnnotation();
  ASSERT_EQ(observer()->annotations(), 4);

  ClickAnnotation(0, true);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return observer()->clicks() == 1;
  }));

  ClickAnnotation(1, true);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return observer()->clicks() == 2;
  }));

  ClickAnnotation(2, true);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return observer()->clicks() == 3;
  }));

  ClickAnnotation(3, true);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return observer()->clicks() == 4;
  }));

  manager->RemoveDecorationsWithType("type1");
  // Check the resulting html is annotating at the right place.
  CheckHtml("<html><body>"
            "<p>abc<span> </span><chrome_annotation>def</chrome_annotation></p>"
            "<p>zzzzz ghi zzzzz</p>"
            "<p>zzzzz<span> </span><chrome_annotation>klm</chrome_annotation> "
            "zzzzz</p>"
            "</body></html>");

  CountAnnotation();
  ASSERT_EQ(observer()->annotations(), 2);

  ClickAnnotation(0, true);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return observer()->clicks() == 5;
  }));

  ClickAnnotation(1, true);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return observer()->clicks() == 6;
  }));

  // Make sure it's back to the original.
  manager->RemoveDecorations();
  CheckHtml(html);
}

// Tests on (simulated) navigation in web state.
TEST_F(AnnotationTextManagerNoViewportTest, NavigationClearsAnnotation) {
  std::string text1 = "<html><body>"
                      "<p>text</p>"
                      "<p>annotation</p>"
                      "<p>text</p>"
                      "</body></html>";

  LoadHtmlAndExtractText(text1);
  NSString* source = base::SysUTF8ToNSString(observer()->extracted_text());
  CreateAndApplyAnnotationsWithTypes(
      source,
      @{@"type1" : @[ @"annotation" ]}, observer()->seq_id());
  ClickAnnotation(0);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return observer()->clicks() == 1;
  }));
  ASSERT_TRUE(observer()->click_data() == "type1-annotation");

  std::string text2 = "<html><body>"
                      "<p>bla</p>"
                      "<p>blurb</p>"
                      "<p>bla</p>"
                      "</body></html>";
  LoadHtmlAndExtractText(text2);
  source = base::SysUTF8ToNSString(observer()->extracted_text());
  CreateAndApplyAnnotationsWithTypes(
      source,
      @{@"type2" : @[ @"blurb" ]}, observer()->seq_id());
  ClickAnnotation(0);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return observer()->clicks() == 2;
  }));
  ASSERT_TRUE(observer()->click_data() == "type2-blurb");

  // Now navigate back to original text.
  LoadHtmlAndExtractText(text1);
  source = base::SysUTF8ToNSString(observer()->extracted_text());
  CreateAndApplyAnnotationsWithTypes(
      source,
      @{@"type1" : @[ @"annotation" ]}, observer()->seq_id());
  ClickAnnotation(0);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return observer()->clicks() == 3;
  }));
  ASSERT_TRUE(observer()->click_data() == "type1-annotation");
}

// Tests on (simulated) navigation in web state.
TEST_F(AnnotationTextManagerViewportTest, NavigationClearsAnnotation) {
  std::string text1 = "<html><body>"
                      "<p>text</p>"
                      "<p>annotation</p>"
                      "<p>text</p>"
                      "</body></html>";

  LoadHtmlAndExtractText(text1);
  NSString* source = base::SysUTF8ToNSString(observer()->extracted_text());
  CreateAndApplyAnnotationsWithTypes(
      source,
      @{@"type1" : @[ @"annotation" ]}, observer()->seq_id());
  ClickAnnotation(0, true);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return observer()->clicks() == 1;
  }));
  ASSERT_TRUE(observer()->click_data() == "type1-annotation");

  std::string text2 = "<html><body>"
                      "<p>bla</p>"
                      "<p>blurb</p>"
                      "<p>bla</p>"
                      "</body></html>";
  LoadHtmlAndExtractText(text2);
  source = base::SysUTF8ToNSString(observer()->extracted_text());
  CreateAndApplyAnnotationsWithTypes(
      source,
      @{@"type2" : @[ @"blurb" ]}, observer()->seq_id());

  CheckHtml("<html><body>"
            "<p>bla</p>"
            "<p><chrome_annotation>blurb</chrome_annotation></p>"
            "<p>bla</p>"
            "</body></html>");
  ClickAnnotation(0, true);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return observer()->clicks() == 2;
  }));
  ASSERT_TRUE(observer()->click_data() == "type2-blurb");

  // Now navigate back to original text.
  LoadHtmlAndExtractText(text1);
  source = base::SysUTF8ToNSString(observer()->extracted_text());
  CreateAndApplyAnnotationsWithTypes(
      source,
      @{@"type1" : @[ @"annotation" ]}, observer()->seq_id());
  ClickAnnotation(0, true);
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return observer()->clicks() == 3;
  }));
  ASSERT_TRUE(observer()->click_data() == "type1-annotation");
}

}  // namespace web
