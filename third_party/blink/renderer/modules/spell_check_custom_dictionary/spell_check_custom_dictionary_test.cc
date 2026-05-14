// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/spell_check_custom_dictionary/spell_check_custom_dictionary.h"

#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "components/spellcheck/renderer/spellcheck_provider_test.h"
#include "third_party/blink/renderer/core/editing/spellcheck/idle_spell_check_controller.h"
#include "third_party/blink/renderer/core/editing/spellcheck/spell_checker.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/modules/spell_check_custom_dictionary/window_spell_check_custom_dictionary.h"

namespace blink {

namespace {

// A dummy LocalInterfaceProvider that doesn't bind any remote application.
class DummyLocalInterfaceProvider
    : public service_manager::LocalInterfaceProvider {
 public:
  void GetInterface(const std::string& name,
                    mojo::ScopedMessagePipeHandle request_handle) override {}
};

base::FilePath GetHunspellDirectory() {
  base::FilePath hunspell_directory;
  if (!base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT,
                              &hunspell_directory)) {
    return base::FilePath();
  }

  hunspell_directory = hunspell_directory.AppendASCII("third_party");
  hunspell_directory = hunspell_directory.AppendASCII("hunspell_dictionaries");
  return hunspell_directory;
}

class SpellCheckCustomDictionaryTest : public PageTestBase {
 public:
  SpellCheckCustomDictionaryTest() = default;
  ~SpellCheckCustomDictionaryTest() override = default;

  void InitializeSpellCheck(const std::string& unsplit_languages) {
    base::FilePath hunspell_directory = GetHunspellDirectory();
    EXPECT_FALSE(hunspell_directory.empty());
    std::vector<std::string> languages = base::SplitString(
        unsplit_languages, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

    for (const auto& language : languages) {
      base::File file(
          spellcheck::GetVersionedFileName(language, hunspell_directory),
          base::File::FLAG_OPEN | base::File::FLAG_READ);
      spellcheck_->AddSpellcheckLanguage(std::move(file), language);
    }
  }

  void SetUp() override {
    PageTestBase::SetUp(gfx::Size());

    LocalDOMWindow* window = GetDocument().GetFrame()->DomWindow();
    dictionary_ =
        WindowSpellCheckCustomDictionary::spellCheckCustomDictionary(*window);

    spellcheck_ = new SpellCheck(&dummy_provider_);
    provider_ = std::make_unique<TestingSpellCheckProvider>(spellcheck_,
                                                            &dummy_provider_);

    EmptyLocalFrameClient* frame_client =
        static_cast<EmptyLocalFrameClient*>(GetFrame().Client());
    frame_client->SetTextCheckerClientForTesting(Provider());
  }

  void TearDown() override {
    dictionary_ = nullptr;
    spellcheck_ = nullptr;
    provider_ = nullptr;

    EmptyLocalFrameClient* frame_client =
        static_cast<EmptyLocalFrameClient*>(GetFrame().Client());
    frame_client->SetTextCheckerClientForTesting(nullptr);
    PageTestBase::TearDown();
  }

  ScriptState* GetScriptState() {
    return ToScriptStateForMainWorld(GetDocument().GetFrame());
  }

  SpellCheckCustomDictionary* GetDictionary() { return dictionary_; }

  TestingSpellCheckProvider* Provider() { return provider_.get(); }

  WebTextCheckClient* Client() {
    return static_cast<WebTextCheckClient*>(Provider());
  }

 private:
  Persistent<SpellCheckCustomDictionary> dictionary_;
  DummyLocalInterfaceProvider dummy_provider_;

  // Owned by |provider_|.
  raw_ptr<SpellCheck> spellcheck_;
  std::unique_ptr<TestingSpellCheckProvider> provider_;
};

TEST_F(SpellCheckCustomDictionaryTest, AddRemoveWords) {
  InitializeSpellCheck("en-US,es-ES");

  SpellCheckCustomDictionary* dict = GetDictionary();
  ASSERT_NE(dict, nullptr);

  ScriptState* script_state = GetScriptState();
  ScriptState::Scope scope(script_state);

  size_t misspelling_start = 0;
  size_t misspelling_end = 0;

  const WebString& text = WebString("zzzz");
  Client()->CheckSpelling(text, misspelling_start, misspelling_end, nullptr);

  EXPECT_EQ(misspelling_start, 0u);
  EXPECT_EQ(misspelling_end, 4u);

  dict->addWords(script_state, {"zzzz"});
  Client()->CheckSpelling(text, misspelling_start, misspelling_end, nullptr);
  EXPECT_EQ(misspelling_start, 0u);
  EXPECT_EQ(misspelling_end, 0u);

  dict->removeWords(script_state, {"zzzz"});
  Client()->CheckSpelling(text, misspelling_start, misspelling_end, nullptr);
  EXPECT_EQ(misspelling_start, 0u);
  EXPECT_EQ(misspelling_end, 4u);
}

// removeWords() should schedule a hot-mode spell-check pass so that squiggles
// under the now-misspelled words appear without waiting for the user to type
// in each editable. The kick is gated by transient user activation as a
// privacy mitigation. This test has activation present.
TEST_F(SpellCheckCustomDictionaryTest,
       RemoveWordsKicksIdleControllerWithUserActivation) {
  InitializeSpellCheck("en-US");

  SpellCheckCustomDictionary* dict = GetDictionary();
  ASSERT_NE(dict, nullptr);

  ScriptState* script_state = GetScriptState();
  ScriptState::Scope scope(script_state);

  // Simulate a user gesture so RespondToChangedContents isn't gated out.
  LocalFrame::NotifyUserActivation(
      &GetFrame(), mojom::UserActivationNotificationType::kTest);
  ASSERT_TRUE(LocalFrame::HasTransientUserActivation(&GetFrame()));

  IdleSpellCheckController& idle =
      GetFrame().GetSpellChecker().GetIdleSpellCheckController();
  using State = IdleSpellCheckController::State;

  // Force the controller out of the post-init kHotModeRequested-ish state so
  // we can see removeWords transition it back.
  idle.Deactivate();
  ASSERT_EQ(idle.GetState(), State::kInactive);

  dict->removeWords(script_state, {"zzzz"});

  EXPECT_EQ(idle.GetState(), State::kHotModeRequested);
}

// Without a user gesture, removeWords still updates the renderer-side
// dictionary state but the recheck kick must silently no-op.
TEST_F(SpellCheckCustomDictionaryTest,
       RemoveWordsWithoutUserActivationDoesNotCrash) {
  InitializeSpellCheck("en-US");

  SpellCheckCustomDictionary* dict = GetDictionary();
  ASSERT_NE(dict, nullptr);

  ScriptState* script_state = GetScriptState();
  ScriptState::Scope scope(script_state);

  ASSERT_FALSE(LocalFrame::HasTransientUserActivation(&GetFrame()));

  IdleSpellCheckController& idle =
      GetFrame().GetSpellChecker().GetIdleSpellCheckController();
  using State = IdleSpellCheckController::State;
  idle.Deactivate();
  ASSERT_EQ(idle.GetState(), State::kInactive);

  // Should not crash, should not transition to kHotModeRequested.
  dict->removeWords(script_state, {"zzzz"});

  EXPECT_NE(idle.GetState(), State::kHotModeRequested);
}

}  // namespace
}  // namespace blink
