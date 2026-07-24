// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/spell_check_custom_dictionary/spell_check_custom_dictionary.h"

#include "base/containers/span.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "components/spellcheck/renderer/spellcheck_provider_test.h"
#include "third_party/blink/public/web/web_text_check_client.h"
#include "third_party/blink/renderer/core/editing/spellcheck/idle_spell_check_controller.h"
#include "third_party/blink/renderer/core/editing/spellcheck/spell_checker.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/modules/spell_check_custom_dictionary/document_spell_check_custom_dictionary.h"

namespace blink {

namespace {

// A dummy LocalInterfaceProvider that doesn't bind any remote application.
class DummyLocalInterfaceProvider
    : public service_manager::LocalInterfaceProvider {
 public:
  void GetInterface(const std::string& name,
                    mojo::ScopedMessagePipeHandle request_handle) override {}
};

// A WebTextCheckClient that records the exact word lists forwarded by
// SpellCheckCustomDictionary::{addWords,removeWords}.
class RecordingTextCheckClient : public WebTextCheckClient {
 public:
  bool IsSpellCheckingEnabled() const override { return true; }

  void SpellCheckCustomDictionaryChanged(
      const std::vector<std::string>& words_added,
      const std::vector<std::string>& words_removed) override {
    last_added_ = words_added;
    last_removed_ = words_removed;
    ++change_count_;
  }

  std::vector<std::string> last_added_;
  std::vector<std::string> last_removed_;
  int change_count_ = 0;
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

    dictionary_ =
        DocumentSpellCheckCustomDictionary::spellCheckCustomDictionary(
            GetDocument());

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

  // Swaps in a client that records the forwarded word lists so tests can assert
  // on what addWords()/removeWords() emit after sanitization.
  void UseRecordingClient(RecordingTextCheckClient* client) {
    static_cast<EmptyLocalFrameClient*>(GetFrame().Client())
        ->SetTextCheckerClientForTesting(client);
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

// A well-formed word is forwarded to the client unchanged.
TEST_F(SpellCheckCustomDictionaryTest, AddWordForwardsValidWord) {
  SpellCheckCustomDictionary* dict = GetDictionary();
  ASSERT_NE(dict, nullptr);
  ScriptState* script_state = GetScriptState();
  ScriptState::Scope scope(script_state);

  RecordingTextCheckClient client;
  UseRecordingClient(&client);

  dict->addWords(script_state, {"zzzz"});

  ASSERT_EQ(client.last_added_.size(), 1u);
  EXPECT_EQ(client.last_added_[0], "zzzz");
}

// Empty words are dropped before reaching the client.
TEST_F(SpellCheckCustomDictionaryTest, AddWordRejectsEmptyWord) {
  SpellCheckCustomDictionary* dict = GetDictionary();
  ASSERT_NE(dict, nullptr);
  ScriptState* script_state = GetScriptState();
  ScriptState::Scope scope(script_state);

  RecordingTextCheckClient client;
  UseRecordingClient(&client);

  dict->addWords(script_state, {""});

  EXPECT_TRUE(client.last_added_.empty());
}

// Words padded with leading or trailing ASCII whitespace are rejected.
TEST_F(SpellCheckCustomDictionaryTest, AddWordRejectsAsciiWhitespacePadding) {
  SpellCheckCustomDictionary* dict = GetDictionary();
  ASSERT_NE(dict, nullptr);
  ScriptState* script_state = GetScriptState();
  ScriptState::Scope scope(script_state);

  RecordingTextCheckClient client;
  UseRecordingClient(&client);

  dict->addWords(script_state, {" zzzz", "zzzz\t", " zzzz "});

  EXPECT_TRUE(client.last_added_.empty());
}

// Unlike the browser dictionary's ASCII-only trim, Unicode space separators in
// the bidi WhiteSpace class are also stripped and therefore rejected as
// padding.
TEST_F(SpellCheckCustomDictionaryTest, AddWordRejectsUnicodeWhitespacePadding) {
  SpellCheckCustomDictionary* dict = GetDictionary();
  ASSERT_NE(dict, nullptr);
  ScriptState* script_state = GetScriptState();
  ScriptState::Scope scope(script_state);

  RecordingTextCheckClient client;
  UseRecordingClient(&client);

  dict->addWords(script_state, {String(u"\u3000zzzz\u3000")});

  EXPECT_TRUE(client.last_added_.empty());
}

// Conversely, U+00A0 NO-BREAK SPACE is not treated as whitespace.
TEST_F(SpellCheckCustomDictionaryTest, AddWordAllowsNoBreakSpace) {
  SpellCheckCustomDictionary* dict = GetDictionary();
  ASSERT_NE(dict, nullptr);
  ScriptState* script_state = GetScriptState();
  ScriptState::Scope scope(script_state);

  RecordingTextCheckClient client;
  UseRecordingClient(&client);

  dict->addWords(script_state, {String(u"\u00A0zzzz\u00A0")});

  ASSERT_EQ(client.last_added_.size(), 1u);
  EXPECT_EQ(client.last_added_[0], "\xC2\xA0zzzz\xC2\xA0");
}

// Only edge whitespace is rejected; internal whitespace is preserved.
TEST_F(SpellCheckCustomDictionaryTest, AddWordAllowsInternalWhitespace) {
  SpellCheckCustomDictionary* dict = GetDictionary();
  ASSERT_NE(dict, nullptr);
  ScriptState* script_state = GetScriptState();
  ScriptState::Scope scope(script_state);

  RecordingTextCheckClient client;
  UseRecordingClient(&client);

  dict->addWords(script_state, {"foo bar"});

  ASSERT_EQ(client.last_added_.size(), 1u);
  EXPECT_EQ(client.last_added_[0], "foo bar");
}

// A word containing an unpaired surrogate is ill-formed UTF-16 and is rejected
// rather than stored as a U+FFFD-mangled entry that can never match real text.
TEST_F(SpellCheckCustomDictionaryTest, AddWordRejectsUnpairedSurrogate) {
  SpellCheckCustomDictionary* dict = GetDictionary();
  ASSERT_NE(dict, nullptr);
  ScriptState* script_state = GetScriptState();
  ScriptState::Scope scope(script_state);

  RecordingTextCheckClient client;
  UseRecordingClient(&client);

  // "zzzz" followed by a lone high surrogate.
  const UChar kSurrogateWord[] = {'z', 'z', 'z', 'z', 0xD800};
  dict->addWords(script_state, {String(base::span(kSurrogateWord))});

  EXPECT_TRUE(client.last_added_.empty());
}

// A valid surrogate pair (an astral-plane character such as an emoji) is
// well-formed UTF-16 and must be accepted, forwarded verbatim as 4-byte UTF-8.
TEST_F(SpellCheckCustomDictionaryTest, AddWordAllowsValidSurrogatePair) {
  SpellCheckCustomDictionary* dict = GetDictionary();
  ASSERT_NE(dict, nullptr);
  ScriptState* script_state = GetScriptState();
  ScriptState::Scope scope(script_state);

  RecordingTextCheckClient client;
  UseRecordingClient(&client);

  // U+1F600 GRINNING FACE (the surrogate pair D83D DE00).
  dict->addWords(script_state, {String(u"😀")});

  ASSERT_EQ(client.last_added_.size(), 1u);
  EXPECT_EQ(client.last_added_[0], "\xF0\x9F\x98\x80");
}

// A single batch keeps the valid words in order and drops the invalid ones.
TEST_F(SpellCheckCustomDictionaryTest, AddWordsDropsInvalidKeepsValid) {
  SpellCheckCustomDictionary* dict = GetDictionary();
  ASSERT_NE(dict, nullptr);
  ScriptState* script_state = GetScriptState();
  ScriptState::Scope scope(script_state);

  RecordingTextCheckClient client;
  UseRecordingClient(&client);

  dict->addWords(script_state, {"", " skip ", "keep", "foo bar"});

  ASSERT_EQ(client.last_added_.size(), 2u);
  EXPECT_EQ(client.last_added_[0], "keep");
  EXPECT_EQ(client.last_added_[1], "foo bar");
}

// removeWords() rejects unpaired surrogates just like addWords().
TEST_F(SpellCheckCustomDictionaryTest, RemoveWordRejectsUnpairedSurrogate) {
  SpellCheckCustomDictionary* dict = GetDictionary();
  ASSERT_NE(dict, nullptr);
  ScriptState* script_state = GetScriptState();
  ScriptState::Scope scope(script_state);

  RecordingTextCheckClient client;
  UseRecordingClient(&client);

  const UChar kSurrogateWord[] = {'z', 'z', 'z', 'z', 0xD800};
  dict->removeWords(script_state, {String(base::span(kSurrogateWord))});

  EXPECT_TRUE(client.last_removed_.empty());
}

// Unlike addWords(), removeWords() deliberately does NOT drop empty or
// whitespace-padded words: addWords() never stores such words, so a removal
// request for one matches nothing and is a harmless no-op, and their UTF-8
// conversion is identity so they can't collide with a stored entry. They are
// therefore forwarded verbatim.
TEST_F(SpellCheckCustomDictionaryTest, RemoveWordForwardsEmptyAndWhitespace) {
  SpellCheckCustomDictionary* dict = GetDictionary();
  ASSERT_NE(dict, nullptr);
  ScriptState* script_state = GetScriptState();
  ScriptState::Scope scope(script_state);

  RecordingTextCheckClient client;
  UseRecordingClient(&client);

  dict->removeWords(script_state, {"", " zzzz ", "\tzzzz"});

  ASSERT_EQ(client.last_removed_.size(), 3u);
  EXPECT_EQ(client.last_removed_[0], "");
  EXPECT_EQ(client.last_removed_[1], " zzzz ");
  EXPECT_EQ(client.last_removed_[2], "\tzzzz");
}

// removeWords() transcodes well-formed words with the same mode as addWords()
// so a word added and later removed produces identical bytes on both paths.
TEST_F(SpellCheckCustomDictionaryTest, RemoveWordTranscodesLikeAdd) {
  SpellCheckCustomDictionary* dict = GetDictionary();
  ASSERT_NE(dict, nullptr);
  ScriptState* script_state = GetScriptState();
  ScriptState::Scope scope(script_state);

  RecordingTextCheckClient client;
  UseRecordingClient(&client);

  // U+1F600 GRINNING FACE (surrogate pair D83D DE00) round-trips as 4-byte
  // UTF-8 on both the add and remove paths.
  const String emoji{u"😀"};

  dict->addWords(script_state, {emoji});
  ASSERT_EQ(client.last_added_.size(), 1u);
  // Capture the added bytes; removeWords() overwrites last_added_ with its own
  // (empty) words_added list.
  const std::string added_bytes = client.last_added_[0];
  EXPECT_EQ(added_bytes, "\xF0\x9F\x98\x80");

  dict->removeWords(script_state, {emoji});
  ASSERT_EQ(client.last_removed_.size(), 1u);
  EXPECT_EQ(client.last_removed_[0], added_bytes);
}

}  // namespace
}  // namespace blink
