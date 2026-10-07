// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/web_test/renderer/spell_check_client.h"

#include <stddef.h>

#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "content/web_test/renderer/web_test_grammar_checker.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_text_checking_completion.h"
#include "third_party/blink/public/web/web_text_checking_result.h"

namespace content {

SpellCheckClient::SpellCheckClient(blink::WebLocalFrame* frame)
    : frame_(frame) {}

SpellCheckClient::~SpellCheckClient() {
  // v8::Persistent will leak on destroy, due to the default
  // NonCopyablePersistentTraits (it claims this may change in the future).
  resolved_callback_.Reset();
}

void SpellCheckClient::SetEnabled(bool enabled) {
  enabled_ = enabled;
}

void SpellCheckClient::Reset() {
  enabled_ = false;
  resolved_callback_.Reset();
  ClearDocumentCustomWords();
}

void SpellCheckClient::ClearDocumentCustomWords() {
  document_custom_words_.clear();
}

bool SpellCheckClient::IsSpellCheckingEnabled() const {
  // Ensure that the spellchecker code paths are always tested in web tests.
  return true;
}

void SpellCheckClient::CheckSpelling(
    const blink::WebString& text,
    size_t& misspelled_offset,
    size_t& misspelled_length,
    std::vector<blink::WebString>* optional_suggestions) {
  if (!enabled_) {
    misspelled_offset = 0;
    misspelled_length = 0;
    return;
  }

  // Check the spelling of the given text.
  spell_checker_.SpellCheckWord(text, &misspelled_offset, &misspelled_length);
  if (IsDocumentCustomWord(text.Utf16(), misspelled_offset,
                           misspelled_length)) {
    misspelled_offset = 0;
    misspelled_length = 0;
  }
}

void SpellCheckClient::RequestCheckingOfText(
    const blink::WebString& text,
    const std::vector<blink::WebSpellingMarker>& spelling_markers,
    blink::WebTextCheckClient::ShouldForceRefreshTextCheckService
        should_force_refresh,
    std::unique_ptr<blink::WebTextCheckingCompletion> completion) {
  if (!enabled_ || text.IsEmpty()) {
    if (completion) {
      completion->DidCancelCheckingText();
      RequestResolved();
    }
    return;
  }

  if (last_requested_text_checking_completion_) {
    last_requested_text_checking_completion_->DidCancelCheckingText();
    last_requested_text_checking_completion_.reset();
    RequestResolved();
  }

  last_requested_text_checking_completion_ = std::move(completion);
  last_requested_text_check_string_ = text;
  if (should_force_refresh ==
          blink::WebTextCheckClient::ShouldForceRefreshTextCheckService::kNo &&
      spell_checker_.HasInCache(text)) {
    FinishLastTextCheck();
  } else {
    frame_->GetTaskRunner(blink::TaskType::kInternalTest)
        ->PostTask(FROM_HERE,
                   base::BindOnce(&SpellCheckClient::FinishLastTextCheck,
                                  weak_factory_.GetWeakPtr()));
  }
}

void SpellCheckClient::FinishLastTextCheck() {
  if (!last_requested_text_checking_completion_)
    return;
  std::vector<blink::WebTextCheckingResult> results;
  size_t offset = 0;
  if (!spell_checker_.IsMultiWordMisspelling(last_requested_text_check_string_,
                                             &results)) {
    std::u16string text = last_requested_text_check_string_.Utf16();
    while (text.length()) {
      size_t misspelled_position = 0;
      size_t misspelled_length = 0;
      spell_checker_.SpellCheckWord(blink::WebString::FromUtf16(text),
                                    &misspelled_position, &misspelled_length);
      if (!misspelled_length)
        break;
      if (IsDocumentCustomWord(text, misspelled_position, misspelled_length)) {
        text = text.substr(misspelled_position + misspelled_length);
        offset += misspelled_position + misspelled_length;
        continue;
      }
      std::vector<blink::WebString> suggestions;
      spell_checker_.FillSuggestionList(
          blink::WebString::FromUtf16(
              text.substr(misspelled_position, misspelled_length)),
          &suggestions);
      results.push_back(blink::WebTextCheckingResult(
          blink::kWebTextDecorationTypeSpelling, offset + misspelled_position,
          misspelled_length, suggestions));
      text = text.substr(misspelled_position + misspelled_length);
      offset += misspelled_position + misspelled_length;
    }
    WebTestGrammarChecker::CheckGrammarOfString(
        last_requested_text_check_string_, &results);
  }
  last_requested_text_checking_completion_->DidFinishCheckingText(results);
  last_requested_text_checking_completion_.reset();
  RequestResolved();
}

void SpellCheckClient::SpellCheckCustomDictionaryChanged(
    const std::vector<std::string>& words_added,
    const std::vector<std::string>& words_removed) {
  for (const std::string& word : words_removed) {
    document_custom_words_.erase(base::UTF8ToUTF16(word));
  }
  std::vector<blink::WebString> added;
  added.reserve(words_added.size());
  for (const std::string& word : words_added) {
    if (document_custom_words_.insert(base::UTF8ToUTF16(word)).second) {
      added.push_back(blink::WebString::FromUtf8(word));
    }
  }
  // Like SpellCheckProvider, drop existing markers under the added words.
  // Blink re-checks after removals itself.
  if (!added.empty()) {
    frame_->RemoveSpellingMarkersUnderWords(added);
  }
}

std::vector<blink::WebString>
SpellCheckClient::GetSpellCheckCustomDictionaryWords() const {
  return base::ToVector(document_custom_words_, &blink::WebString::FromUtf16);
}

bool SpellCheckClient::IsDocumentCustomWord(const std::u16string& text,
                                            size_t offset,
                                            size_t length) const {
  return length && document_custom_words_.contains(text.substr(offset, length));
}

void SpellCheckClient::SetSpellCheckResolvedCallback(
    v8::Local<v8::Function> callback) {
  resolved_callback_.Reset(frame_->GetAgentGroupScheduler()->Isolate(),
                           callback);
}

void SpellCheckClient::RemoveSpellCheckResolvedCallback() {
  resolved_callback_.Reset();
}

void SpellCheckClient::RequestResolved() {
  if (resolved_callback_.IsEmpty())
    return;

  v8::Isolate* isolate = frame_->GetAgentGroupScheduler()->Isolate();
  v8::HandleScope handle_scope(isolate);

  v8::Local<v8::Context> context = frame_->MainWorldScriptContext();
  if (context.IsEmpty())
    return;

  v8::Context::Scope context_scope(context);

  frame_->CallFunctionEvenIfScriptDisabled(
      v8::Local<v8::Function>::New(isolate, resolved_callback_),
      context->Global(), 0, nullptr);
}

}  // namespace content
