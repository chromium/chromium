// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/web/web_text_check_client.h"
#include "third_party/blink/public/web/web_text_checking_result.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/spellcheck/spell_check_test_base.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

#if BUILDFLAG(IS_ANDROID)

namespace blink {

namespace {

const char kLongText[] =
    "This is sentence 01. This is sentence 02. This is sentence 03. "
    "This is sentence 04. This is sentence 05. This is sentence 06. "
    "This is sentence 07. This is sentence 08. This is sentence 09. "
    "This is sentence 10. This is sentence 11. This is sentence 12. "
    "This is sentence 13. This is sentence 14. This is sentence 15. "
    "This is sentence 16. This is sentence 17. This is sentence 18. "
    "This is sentence 19. This is sentence 20. This is sentence 21. "
    "This is sentence 22. This is sentence 23. This is sentence 24. "
    "This is sentence 25. This is sentence 26. This is sentence 27. "
    "This is sentence 28. This is sentence 29. This is sentence 30. "
    "This is sentence 31. This is sentence 32. This is sentence 33. "
    "This is sentence 34. This is sentence 35. This is sentence 36. "
    "This is sentence 37. This is sentence 38. This is sentence 39. "
    "This is sentence 40.";

}  // namespace

class FullContentSpellCheckMockClient : public WebTextCheckClient {
 public:
  void RequestCheckingOfText(
      const WebString& text,
      const std::vector<WebSpellingMarker>& markers,
      WebTextCheckClient::ShouldForceRefreshTextCheckService force_refresh,
      std::unique_ptr<WebTextCheckingCompletion> completion) override {
    texts_.push_back(text);
    force_refreshes_.push_back(force_refresh);
    if (completion) {
      completion->DidFinishCheckingText(std::vector<WebTextCheckingResult>());
    }
  }

  bool IsSpellCheckingEnabled() const override { return true; }

  size_t CallCount() const { return texts_.size(); }
  const std::vector<WebString>& GetTexts() const { return texts_; }
  const std::vector<WebTextCheckClient::ShouldForceRefreshTextCheckService>&
  GetForceRefreshes() const {
    return force_refreshes_;
  }

 private:
  std::vector<WebString> texts_;
  std::vector<WebTextCheckClient::ShouldForceRefreshTextCheckService>
      force_refreshes_;
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
  test::RunPendingTasks();

  EXPECT_GT(MockClient().CallCount(), 0u);
  EXPECT_EQ(MockClient().GetForceRefreshes()[0],
            WebTextCheckClient::ShouldForceRefreshTextCheckService::kYes);
}

TEST_F(LocalFrameFullContentSpellCheckTest, NoEditableRoot) {
  SetBodyContent("<div>Hello world</div>");
  UpdateAllLifecyclePhasesForTest();

  GetFrame().PerformFullContentSpellCheck();
  test::RunPendingTasks();

  EXPECT_EQ(MockClient().CallCount(), 0u);
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
  test::RunPendingTasks();

  EXPECT_GT(MockClient().CallCount(), 0u);
  EXPECT_EQ(MockClient().GetForceRefreshes()[0],
            WebTextCheckClient::ShouldForceRefreshTextCheckService::kYes);
}

TEST_F(LocalFrameFullContentSpellCheckTest,
       TriggersFullCheckWithNestedElements) {
  SetBodyContent("<div contenteditable><i>Hello</i> world.</div>");
  Element* div = QuerySelector("div");
  div->Focus();
  UpdateAllLifecyclePhasesForTest();

  GetFrame().PerformFullContentSpellCheck();
  test::RunPendingTasks();

  EXPECT_GT(MockClient().CallCount(), 0u);
  EXPECT_EQ(MockClient().GetForceRefreshes()[0],
            WebTextCheckClient::ShouldForceRefreshTextCheckService::kYes);
}

TEST_F(LocalFrameFullContentSpellCheckTest, TriggersFullCheckWithLongText) {
  SetBodyContent(base::StrCat({"<div contenteditable>", kLongText, "</div>"}));
  Element* div = QuerySelector("div");
  div->Focus();
  UpdateAllLifecyclePhasesForTest();

  GetFrame().PerformFullContentSpellCheck();
  test::RunPendingTasks();

  EXPECT_GT(MockClient().CallCount(), 0u);
  EXPECT_EQ(MockClient().GetForceRefreshes()[0],
            WebTextCheckClient::ShouldForceRefreshTextCheckService::kYes);
}

}  // namespace blink

#endif  // BUILDFLAG(IS_ANDROID)
