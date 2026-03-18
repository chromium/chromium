// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/web/web_text_check_client.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/spellcheck/spell_check_test_base.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"

#if BUILDFLAG(IS_ANDROID)

namespace blink {

class FullContentSpellCheckMockClient : public WebTextCheckClient {
 public:
  void RequestCheckingOfText(
      const WebString& text,
      const std::vector<WebSpellingMarker>& markers,
      WebTextCheckClient::ShouldForceRefreshTextCheckService force_refresh,
      std::unique_ptr<WebTextCheckingCompletion> completion) override {
    last_text_ = text;
    last_force_refresh_ = force_refresh;
    call_count_++;
  }

  bool IsSpellCheckingEnabled() const override { return true; }

  int call_count_ = 0;
  WebString last_text_;
  WebTextCheckClient::ShouldForceRefreshTextCheckService last_force_refresh_ =
      WebTextCheckClient::ShouldForceRefreshTextCheckService::kNo;
};

class LocalFrameFullContentSpellCheckTest : public SpellCheckTestBase {
 protected:
  void SetUp() override {
    SpellCheckTestBase::SetUp();
    feature_list_.InitAndEnableFeature(
        blink::features::kAndroidSpellcheckFullApiBlink);

    mock_client_ = std::make_unique<FullContentSpellCheckMockClient>();
    EmptyLocalFrameClient* frame_client =
        static_cast<EmptyLocalFrameClient*>(GetFrame().Client());
    frame_client->SetTextCheckerClientForTesting(mock_client_.get());
  }

  void TearDown() override {
    EmptyLocalFrameClient* frame_client =
        static_cast<EmptyLocalFrameClient*>(GetFrame().Client());
    frame_client->SetTextCheckerClientForTesting(nullptr);
    SpellCheckTestBase::TearDown();
  }

  FullContentSpellCheckMockClient& MockClient() { return *mock_client_; }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<FullContentSpellCheckMockClient> mock_client_;
};

TEST_F(LocalFrameFullContentSpellCheckTest, TriggersFullCheck) {
  SetBodyContent("<div contenteditable>Hello world</div>");
  Element* div = QuerySelector("div");
  div->Focus();
  UpdateAllLifecyclePhasesForTest();

  GetFrame().PerformFullContentSpellCheck();

  EXPECT_EQ(MockClient().call_count_, 1);
  EXPECT_EQ(MockClient().last_text_, "Hello world");
  EXPECT_EQ(MockClient().last_force_refresh_,
            WebTextCheckClient::ShouldForceRefreshTextCheckService::kYes);
}

TEST_F(LocalFrameFullContentSpellCheckTest, NoEditableRoot) {
  SetBodyContent("<div>Hello world</div>");
  UpdateAllLifecyclePhasesForTest();

  GetFrame().PerformFullContentSpellCheck();

  EXPECT_EQ(MockClient().call_count_, 0);
}

TEST_F(LocalFrameFullContentSpellCheckTest,
       TriggersFullCheckWhenCursorIsMiddleOfWord) {
  SetBodyContent("<div contenteditable>Hello world.</div>");
  Element* div = QuerySelector("div");
  div->Focus();

  // Simulate the cursor at the word "world".
  Node* text = div->firstChild();
  GetFrame().Selection().SetSelection(SelectionInDOMTree::Builder()
                                          .Collapse(Position(text, 8))
                                          .Extend(Position(text, 8))
                                          .Build(),
                                      SetSelectionOptions());
  UpdateAllLifecyclePhasesForTest();

  GetFrame().PerformFullContentSpellCheck();

  EXPECT_EQ(MockClient().call_count_, 1);
  EXPECT_EQ(MockClient().last_text_, "Hello world.");
  EXPECT_EQ(MockClient().last_force_refresh_,
            WebTextCheckClient::ShouldForceRefreshTextCheckService::kYes);
}

TEST_F(LocalFrameFullContentSpellCheckTest,
       TriggersFullCheckWithNestedElements) {
  SetBodyContent("<div contenteditable><i>Hello</i> world.</div>");
  Element* div = QuerySelector("div");
  div->Focus();
  UpdateAllLifecyclePhasesForTest();

  GetFrame().PerformFullContentSpellCheck();

  EXPECT_EQ(MockClient().call_count_, 1);
  EXPECT_EQ(MockClient().last_text_, "Hello world.");
  EXPECT_EQ(MockClient().last_force_refresh_,
            WebTextCheckClient::ShouldForceRefreshTextCheckService::kYes);
}

}  // namespace blink

#endif  // BUILDFLAG(IS_ANDROID)
