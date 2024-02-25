// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WEB_TEST_RENDERER_SPELL_CHECK_CLIENT_H_
#define CONTENT_WEB_TEST_RENDERER_SPELL_CHECK_CLIENT_H_

#include <stdint.h>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/web_test/renderer/web_test_spell_checker.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
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

  // blink::WebSpellCheckClient implementation.
  bool IsSpellCheckingEnabled() const override;
  void CheckSpelling(
      const blink::WebString& text,
      size_t& offset,
      size_t& length,
      blink::WebVector<blink::WebString>* optional_suggestions) override;
  void RequestCheckingOfText(
      const blink::WebString& text,
      std::unique_ptr<blink::WebTextCheckingCompletion> completion) override;

 private:
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

  v8::Persistent<v8::Function> resolved_callback_;

  base::WeakPtrFactory<SpellCheckClient> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_WEB_TEST_RENDERER_SPELL_CHECK_CLIENT_H_
