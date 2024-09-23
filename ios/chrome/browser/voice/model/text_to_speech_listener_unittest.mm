// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/voice/model/text_to_speech_listener.h"

#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/fakes/fake_web_client.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/test/web_state_test_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/test/web_view_content_test_util.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {
const char kExpectedDecodedData[] = "testaudo32oio";
NSString* const kHTMLFormat =
    @"<html><head><script>%@</script></head><body></body></html>";
NSString* const kValidVoiceSearchScript =
    @"(function(){var _a_tts='dGVzdGF1ZG8zMm9pbw==';var _m_tts= {}})";
}  // namespace

#pragma mark - TestTTSListenerDelegate

@interface TestTTSListenerDelegate : NSObject <TextToSpeechListenerDelegate> {
  // Backing objects for properties of the same name.
  NSData* _expectedAudioData;
}

// The expected audio data to be returned by the TextToSpeechListener.
@property(nonatomic, strong) NSData* expectedAudioData;

// Whether `-textToSpeechListener:didReceiveResult:` was called.
@property(nonatomic, assign) BOOL audioDataReceived;

@end

@implementation TestTTSListenerDelegate

@synthesize audioDataReceived = _audioDataReceived;

- (void)setExpectedAudioData:(NSData*)expectedAudioData {
  _expectedAudioData = expectedAudioData;
}

- (NSData*)expectedAudioData {
  return _expectedAudioData;
}

- (void)textToSpeechListener:(TextToSpeechListener*)listener
            didReceiveResult:(NSData*)result {
  EXPECT_NSEQ(self.expectedAudioData, result);
  self.audioDataReceived = YES;
}

- (void)textToSpeechListenerWebStateWasDestroyed:
    (TextToSpeechListener*)listener {
}

- (BOOL)shouldTextToSpeechListener:(TextToSpeechListener*)listener
                  parseDataFromURL:(const GURL&)URL {
  return YES;
}

@end

#pragma mark - TextToSpeechListenerTest

class TextToSpeechListenerTest : public PlatformTest {
 public:
  TextToSpeechListenerTest()
      : web_client_(std::make_unique<web::FakeWebClient>()) {}

  void SetUp() override {
    PlatformTest::SetUp();

    profile_ = TestProfileIOS::Builder().Build();

    web::WebState::CreateParams params(profile_.get());
    web_state_ = web::WebState::Create(params);
    web_state_->GetView();
    web_state_->SetKeepRenderProcessAlive(true);
    // Load Html is triggering several callbacks when called the first time.
    // Call it a first time before setting the `delegate_` to make sure that it
    // is working as expected in tests.
    web::test::LoadHtml(@"<html><body>Page loaded</body></html>", web_state());
    ASSERT_TRUE(
        web::test::WaitForWebViewContainingText(web_state(), "Page loaded"));

    delegate_ = [[TestTTSListenerDelegate alloc] init];
    listener_ = [[TextToSpeechListener alloc] initWithWebState:web_state()
                                                      delegate:delegate_];
  }

  void TestExtraction(NSString* html, NSData* expected_audio_data) {
    [delegate_ setExpectedAudioData:expected_audio_data];
    web::test::LoadHtml(html, web_state());
    ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
        base::Seconds(1000), true, ^bool {
          return [delegate_ audioDataReceived];
        }));
  }

 private:
  web::WebState* web_state() { return web_state_.get(); }

  web::ScopedTestingWebClient web_client_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<web::WebState> web_state_;

  TestTTSListenerDelegate* delegate_;
  TextToSpeechListener* listener_;
};

TEST_F(TextToSpeechListenerTest, ValidAudioDataTest) {
  NSData* expected_audio_data =
      [NSData dataWithBytes:&kExpectedDecodedData[0]
                     length:sizeof(kExpectedDecodedData) - 1];
  TestExtraction(
      [NSString stringWithFormat:kHTMLFormat, kValidVoiceSearchScript],
      expected_audio_data);
}

TEST_F(TextToSpeechListenerTest, InvalidAudioDataTest) {
  NSData* expected_audio_data = nil;
  TestExtraction([NSString stringWithFormat:kHTMLFormat, @""],
                 expected_audio_data);
}
