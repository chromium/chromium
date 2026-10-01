// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WEB_TEST_RENDERER_SPELL_CHECK_CLIENT_H_
#define CONTENT_WEB_TEST_RENDERER_SPELL_CHECK_CLIENT_H_

#include <stdint.h>

#include <set>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/web_test/renderer/web_test_spell_checker.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_text_check_client.h"
#include "v8/include/v8.h"

namespace blink {
class WebLocalFrame;
class WebTextCheckingCompletion;
}  // namespace blink

namespace content {

class SpellCheckClient : public blink::WebTextCheckClient {
 public:
  explicit SpellCheckClient(blink::WebLocalFrame* frame);

  SpellCheckClient(const SpellCheckClient&) = delete;
  SpellCheckClient& operator=(const SpellCheckClient&) = delete;

  ~SpellCheckClient() override;

  void SetEnabled(bool enabled);

  // Sets a callback that will be invoked after each request is revoled.
  void SetSpellCheckResolvedCallback(v8::Local<v8::Function> callback);

  // Remove the above callback. Beware: don't call it inside the callback.
  void RemoveSpellCheckResolvedCallback();

  void Reset();

  // Drops the words added through the SpellCheckCustomDictionary web API. The
  // word set is document-scoped, so this is called when a new document is
  // committed in the frame.
  void ClearDocumentCustomWords();

  // blink::WebSpellCheckClient implementation.
  bool IsSpellCheckingEnabled() const override;
  void CheckSpelling(
      const blink::WebString& text,
      size_t& offset,
      size_t& length,
      std::vector<blink::WebString>* optional_suggestions) override;
  void RequestCheckingOfText(
      const blink::WebString& text,
      const std::vector<blink::WebSpellingMarker>& spelling_markers,
      blink::WebTextCheckClient::ShouldForceRefreshTextCheckService
          should_force_refresh,
      std::unique_ptr<blink::WebTextCheckingCompletion> completion) override;
  void SpellCheckCustomDictionaryChanged(
      const std::vector<std::string>& words_added,
      const std::vector<std::string>& words_removed) override;

 private:
  // Returns true if the misspelling at [offset, offset + length) of |text| is
  // a word added through the SpellCheckCustomDictionary web API.
  bool IsDocumentCustomWord(const std::u16string& text,
                            size_t offset,
                            size_t length) const;

  void FinishLastTextCheck();

  void RequestResolved();

  const raw_ptr<blink::WebLocalFrame> frame_;

  // Do not perform any checking when |enabled_ == false|.
  // Tests related to spell checking should enable it manually.
  bool enabled_ = false;

  // The mock spellchecker used in CheckSpelling().
  WebTestSpellChecker spell_checker_;

  blink::WebString last_requested_text_check_string_;
  std::unique_ptr<blink::WebTextCheckingCompletion>
      last_requested_text_checking_completion_;

  // Words added through the SpellCheckCustomDictionary web API. Misspellings
  // matching one of these words are not reported, mirroring
  // SpellCheckProvider in //components/spellcheck.
  std::set<std::u16string> document_custom_words_;

  v8::Persistent<v8::Function> resolved_callback_;

  base::WeakPtrFactory<SpellCheckClient> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_WEB_TEST_RENDERER_SPELL_CHECK_CLIENT_H_
