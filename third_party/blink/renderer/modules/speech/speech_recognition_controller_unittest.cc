// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/speech/speech_recognition_controller.h"

#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/modules/speech/speech_grammar_list.h"

namespace blink {

class SpeechRecognitionControllerTest : public PageTestBase {
 public:
  SpeechRecognitionControllerTest() = default;
  SpeechRecognitionControllerTest(const SpeechRecognitionControllerTest&) =
      delete;
  SpeechRecognitionControllerTest& operator=(
      const SpeechRecognitionControllerTest&) = delete;
  ~SpeechRecognitionControllerTest() override = default;

  void SetUp() override {
    PageTestBase::SetUp();
    context_ = MakeGarbageCollected<SpeechRecognitionContext>(nullptr);
    controller_ = SpeechRecognitionController::From(*GetFrame().DomWindow());
  }

  void TearDown() override {
    context_ = nullptr;
    controller_ = nullptr;
    PageTestBase::TearDown();
  }

  media::mojom::blink::StartSpeechRecognitionRequestParamsPtr BuildParams() {
    SpeechGrammarList* grammars = MakeGarbageCollected<SpeechGrammarList>();
    return controller_->BuildStartSpeechRecognitionRequestParams(
        remote_.InitWithNewPipeAndPassReceiver(),
        receiver_.InitWithNewPipeAndPassRemote(), *grammars, context_, "en-US",
        /*continuous=*/true, /*interim_results=*/true, /*max_alternatives=*/5,
        /*on_device=*/true, /*allow_cloud_fallback=*/true);
  }

  void SetSpeechRecognitionContext() {
    SpeechRecognitionPhraseList* list =
        MakeGarbageCollected<SpeechRecognitionPhraseList>();
    list->addItem(MakeGarbageCollected<SpeechRecognitionPhrase>("text", 2.0));
    context_->setPhrases(list);
  }

 private:
  Persistent<SpeechRecognitionContext> context_;
  Persistent<SpeechRecognitionController> controller_;
  mojo::PendingRemote<media::mojom::blink::SpeechRecognitionSession> remote_;
  mojo::PendingReceiver<media::mojom::blink::SpeechRecognitionSessionClient>
      receiver_;
};

TEST_F(SpeechRecognitionControllerTest, BuildParams) {
  auto params = BuildParams();
  EXPECT_TRUE(params->recognition_context.is_null());
  EXPECT_EQ(params->language, "en-US");
  EXPECT_EQ(params->max_hypotheses, (unsigned int)5);
  EXPECT_TRUE(params->continuous);
  EXPECT_TRUE(params->interim_results);
  EXPECT_TRUE(params->on_device);
  EXPECT_TRUE(params->allow_cloud_fallback);
  EXPECT_TRUE(params->client.is_valid());
  EXPECT_TRUE(params->session_receiver.is_valid());
  EXPECT_FALSE(params->audio_forwarder.is_valid());
}

TEST_F(SpeechRecognitionControllerTest,
       BuildParamsWithSpeechRecognitionContext) {
  SetSpeechRecognitionContext();
  auto params = BuildParams();
  EXPECT_FALSE(params->recognition_context.is_null());
  EXPECT_EQ(params->recognition_context->phrases.size(), (unsigned int)1);
  EXPECT_EQ(params->recognition_context->phrases.at(0)->phrase, "text");
  EXPECT_EQ(params->recognition_context->phrases.at(0)->boost, 2.0);
}

}  // namespace blink
