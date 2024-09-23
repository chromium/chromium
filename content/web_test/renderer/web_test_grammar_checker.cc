// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/web_test/renderer/web_test_grammar_checker.h"

#include <stddef.h>

#include "base/check.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_text_checking_result.h"

namespace {

bool IsASCIIAlpha(char ch) {
  return base::IsAsciiLower(ch | 0x20);
}

}  // namespace

namespace content {

bool WebTestGrammarChecker::CheckGrammarOfString(
    const blink::WebString& text,
    std::vector<blink::WebTextCheckingResult>* results) {
  DCHECK(results);
  std::u16string string_text = text.Utf16();
  if (base::ranges::none_of(string_text, IsASCIIAlpha))
    return true;

  // Find matching grammatical errors from known ones. This function has to
  // check all errors because the given text may consist of two or more
  // sentences that have grammatical errors.
  static const struct {
    const char* text;
    int location;
    int length;
  } kGrammarErrors[] = {
      {"I have a issue.", 7, 1},
      {"I have an grape.", 7, 2},
      {"I have an kiwi.", 7, 2},
      {"I have an muscat.", 7, 2},
      {"You has the right.", 4, 3},
      {"apple orange zz.", 0, 16},
      {"apple zz orange.", 0, 16},
      {"apple,zz,orange.", 0, 16},
      {"orange,zz,apple.", 0, 16},
      {"the the adlj adaasj sdklj. there there", 4, 3},
      {"the the adlj adaasj sdklj. there there", 33, 5},
      {"zz apple orange.", 0, 16},
  };
  for (size_t i = 0; i < std::size(kGrammarErrors); ++i) {
    size_t offset = 0;
    std::u16string error(
        kGrammarErrors[i].text,
        kGrammarErrors[i].text + strlen(kGrammarErrors[i].text));
    while ((offset = string_text.find(error, offset)) != std::u16string::npos) {
      results->push_back(blink::WebTextCheckingResult(
          blink::kWebTextDecorationTypeGrammar,
          offset + kGrammarErrors[i].location, kGrammarErrors[i].length));
      offset += kGrammarErrors[i].length;
    }
  }
  return false;
}

}  // namespace content
