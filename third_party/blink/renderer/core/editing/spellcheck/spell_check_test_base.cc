// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/spellcheck/spell_check_test_base.h"

#include "base/no_destructor.h"
#include "third_party/blink/public/web/web_text_check_client.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"

namespace blink {

namespace {

class EnabledTextCheckerClient : public WebTextCheckClient {
 public:
  EnabledTextCheckerClient() = default;
  ~EnabledTextCheckerClient() override = default;
  bool IsSpellCheckingEnabled() const override { return true; }
};

EnabledTextCheckerClient* GetEnabledTextCheckerClient() {
  static base::NoDestructor<EnabledTextCheckerClient> client;
  return client.get();
}

}  // namespace

void SpellCheckTestBase::SetUp() {
  EditingTestBase::SetUp();

  EmptyLocalFrameClient* frame_client =
      static_cast<EmptyLocalFrameClient*>(GetFrame().Client());
  frame_client->SetTextCheckerClientForTesting(GetEnabledTextCheckerClient());
}

SpellChecker& SpellCheckTestBase::GetSpellChecker() const {
  return GetFrame().GetSpellChecker();
}

}  // namespace blink
