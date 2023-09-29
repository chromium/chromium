// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <WebKit/WebKit.h>

#import "base/memory/ptr_util.h"
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

const char kScriptName[] = "annotations_test";

// Feature to include test ts code only.
class AnnotationsTestJavaScriptFeature : public JavaScriptFeature {
 public:
  AnnotationsTestJavaScriptFeature()
      : JavaScriptFeature(
            ContentWorld::kIsolatedWorld,
            {FeatureScript::CreateWithFilename(
                kScriptName,
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
  TestAnnotationTextObserver() : successes_(0), annotations_(0), clicks_(0) {}

  TestAnnotationTextObserver(const TestAnnotationTextObserver&) = delete;
  TestAnnotationTextObserver& operator=(const TestAnnotationTextObserver&) =
      delete;

  void OnTextExtracted(WebState* web_state,
                       const std::string& text,
                       int seq_id,
                       const base::Value::Dict& metadata) override {
    extracted_text_ = text;
    seq_id_ = seq_id;
    metadata_ = metadata.Clone();
  }

  void OnDecorated(WebState* web_state,
                   int successes,
                   int annotations) override {
    successes_ = successes;
    annotations_ = annotations;
  }

  void OnClick(WebState* web_state,
               const std::string& text,
               CGRect rect,
               const std::string& data) override {
    clicks_++;
    click_data_ = data;
  }

  const std::string& extracted_text() const { return extracted_text_; }
  int successes() const { return successes_; }
  int annotations() const { return annotations_; }
  int clicks() const { return clicks_; }
  int seq_id() const { return seq_id_; }
  const base::Value::Dict& metadata() const { return metadata_; }
  const std::string& click_data() const { return click_data_; }
  void SetAnnotations(int count) { annotations_ = count; }

 private:
  std::string extracted_text_, click_data_;
  int successes_, annotations_, clicks_, seq_id_;
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

    WKWebViewConfigurationProvider& configuration_provider =
        WKWebViewConfigurationProvider::FromBrowserState(GetBrowserState());
    // Force the creation of the content worlds.
    configuration_provider.GetWebViewConfiguration();

    content_world_ =
        JavaScriptFeatureManager::FromBrowserState(GetBrowserState())
            ->GetContentWorldForFeature(
                AnnotationsJavaScriptFeature::GetInstance());

    // Inject ts test helpers functions.
    content_world_->AddFeature(&js_test_feature_);
  }

  void TearDown() override {
    auto* manager = AnnotationsTextManager::FromWebState(web_state());
    manager->RemoveObserver(&observer_);
    WebTestWithWebState::TearDown();
  }

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
  void LoadHtmlAndExtractText(const std::string& html) {
    int seq_id = observer()->seq_id();
    ASSERT_TRUE(LoadHtml(html));
    ASSERT_TRUE(WaitForWebFramesCount(1));

    // Wait for text extracted, background parsing and decoration.
    // Make timeout 3 times the regular action timeout to reduce flakiness.
    EXPECT_TRUE(WaitUntilConditionOrTimeout(3 * kWaitForActionTimeout, ^{
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
    // Create annotation.
    base::Value::List annotations;
    for (NSString* type in items) {
      for (NSString* item in items[type]) {
        NSRange range = [source rangeOfString:item];
        annotations.Append(web::ConvertMatchToAnnotation(
            source, range, [NSString stringWithFormat:@"%@-%@", type, item],
            type));
      }
    }
    auto* manager = AnnotationsTextManager::FromWebState(web_state());
    base::Value value = base::Value(std::move(annotations));
    manager->DecorateAnnotations(web_state(), value, seq_id);

    EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^{
      return observer()->annotations() > 0;
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
  void ClickAnnotation(int index) {
    const base::TimeDelta kCallJavascriptFunctionTimeout =
        kWaitForJSCompletionTimeout;
    __block bool message_received = false;
    base::Value::List params;
    params.Append(index);
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
  JavaScriptContentWorld* content_world_;
  TestAnnotationTextObserver observer_;
  AnnotationsTestJavaScriptFeature js_test_feature_;
};

// Tests page text extraction.
// Covers: PageLoaded, OnTextExtracted, StartExtractingText.
TEST_F(AnnotationTextManagerTest, ExtractText) {
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

TEST_F(AnnotationTextManagerTest, CheckMetadata) {
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

TEST_F(AnnotationTextManagerTest, CheckNoMetadata) {
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

// Tests page decoration when page doesn't change.
// Covers: DecorateAnnotations, ConvertMatchToAnnotation.
TEST_F(AnnotationTextManagerTest, DecorateText) {
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

// Tests page decoration on no-decoration tags.
// Covers: DecorateAnnotations, ConvertMatchToAnnotation.
TEST_F(AnnotationTextManagerTest, NoDecorateText) {
  LoadHtmlAndExtractText("<html><body>"
                         "<p>text</p>"
                         "<a>annotation1</a>"
                         "<input type=\"radio\">"
                         "<label>annotation2</label>"
                         "<p>text</p>"
                         "</body></html>");

  std::string text = "text"
                     "annotation1"
                     "annotation2"
                     "\ntext";
  EXPECT_EQ(text, observer()->extracted_text());

  // Create annotation.
  NSString* source = base::SysUTF8ToNSString(text);
  CreateAndApplyAnnotations(source, @[ @"annotation1", @"annotation2" ],
                            observer() -> seq_id());

  EXPECT_EQ(observer()->successes(), 0);
  EXPECT_EQ(observer()->annotations(), 2);
}

// Tests different annotation cases, including tags boundaries.
// Covers: RemoveDecorations
TEST_F(AnnotationTextManagerTest, DecorateTextCrossingElements) {
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

// Tests annotation cases with line breaks, including tags boundaries.
// Covers: DecorateAnnotations, RemoveDecorations
TEST_F(AnnotationTextManagerTest, DecorateTextBreakElements) {
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

// Tests on click handler.
// Covers: OnClick.
TEST_F(AnnotationTextManagerTest, ClickAnnotation) {
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

// Tests removing annotation of one type
TEST_F(AnnotationTextManagerTest, RemoveDecorationTypeTest) {
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

// Tests on (simulated) navigation in web state.
TEST_F(AnnotationTextManagerTest, NavigationClearsAnnotation) {
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

}  // namespace web
