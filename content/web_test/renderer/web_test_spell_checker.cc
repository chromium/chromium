// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/web_test/renderer/web_test_spell_checker.h"

#include <stddef.h>

#include <algorithm>
#include <array>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/strings/string_util.h"

namespace content {

namespace {

void Append(std::vector<blink::WebString>* data, const blink::WebString& item) {
  std::vector<blink::WebString> result(data->size() + 1);
  for (size_t i = 0; i < data->size(); ++i)
    result[i] = (*data)[i];
  result[data->size()] = item;
  data->swap(result);
}

bool IsASCIIAlpha(char ch) {
  return base::IsAsciiLower(ch | 0x20);
}

}  // namespace

WebTestSpellChecker::WebTestSpellChecker() = default;
WebTestSpellChecker::~WebTestSpellChecker() = default;

bool WebTestSpellChecker::SpellCheckWord(const blink::WebString& text,
                                         size_t* misspelled_offset,
                                         size_t* misspelled_length) {
  DCHECK(misspelled_offset);
  DCHECK(misspelled_length);

  // Reset the result values as our spellchecker does.
  *misspelled_offset = 0;
  *misspelled_length = 0;

  // Convert to a std::u16string because we store std::u16string instances in
  // misspelled_words_ and blink::WebString has no find().
  std::u16string string_text = text.Utf16();
  int skipped_length = 0;

  while (!string_text.empty()) {
    // Extract the first possible English word from the given string.
    // The given string may include non-ASCII characters or numbers. So, we
    // should filter out such characters before start looking up our
    // misspelled-word table.
    // (This is a simple version of our SpellCheckWordIterator class.)
    // If the given string doesn't include any ASCII characters, we can treat
    // the string as valid one.
    std::u16string::iterator first_char =
        std::ranges::find_if(string_text, IsASCIIAlpha);
    if (first_char == string_text.end())
      return true;
    int word_offset = std::distance(string_text.begin(), first_char);
    int max_word_length = static_cast<int>(string_text.length()) - word_offset;
    int word_length;
    std::u16string word;

    // These words are known misspelled words in web tests. If there are other
    // misspelled words in web tests, please add them in this array.
    static const auto misspelled_words = std::to_array<std::u16string>({
        u"foo",
        u"Foo",
        u"baz",
        u"fo",
        u"LibertyF",
        u"chello",
        u"xxxtestxxx",
        u"XXxxx",
        u"Textx",
        u"blockquoted",
        u"asd",
        u"Lorem",
        u"Nunc",
        u"Curabitur",
        u"eu",
        u"adlj",
        u"adaasj",
        u"sdklj",
        u"jlkds",
        u"jsaada",
        u"jlda",
        u"contentEditable",

        // Prefer to match the full word than a partial word when there's an
        // ambiguous boundary.
        u"zz't",
        u"zz",

        // The following words are used by unit tests.
        u"ifmmp",
        u"qwertyuiopasd",
        u"qwertyuiopasdf",
        u"upper case",
        u"wellcome",
    });

    // Look up our misspelled-word table to check if the extracted word is a
    // known misspelled word, and return the offset and the length of the
    // extracted word if this word is a known misspelled word.
    for (const std::u16string& misspelled_word : misspelled_words) {
      word_length =
          std::min(static_cast<int>(misspelled_word.length()), max_word_length);
      word = string_text.substr(word_offset, word_length);
      if (word == misspelled_word &&
          (static_cast<int>(string_text.length()) ==
               word_offset + word_length ||
           !IsASCIIAlpha(string_text[word_offset + word_length]))) {
        *misspelled_offset = word_offset + skipped_length;
        *misspelled_length = word_length;
        break;
      }
    }

    if (*misspelled_length > 0)
      break;

    std::u16string::iterator last_char = std::find_if_not(
        string_text.begin() + word_offset, string_text.end(), IsASCIIAlpha);
    if (last_char == string_text.end())
      word_length = static_cast<int>(string_text.length()) - word_offset;
    else
      word_length = std::distance(first_char, last_char);

    DCHECK_LT(0, word_offset + word_length);
    string_text = string_text.substr(word_offset + word_length);
    skipped_length += word_offset + word_length;
  }

  return false;
}

bool WebTestSpellChecker::HasInCache(const blink::WebString& word) {
  return word == "Spell wellcome. Is it broken?" ||
         word == "Spell wellcome.\x007F";
}

bool WebTestSpellChecker::IsMultiWordMisspelling(
    const blink::WebString& text,
    std::vector<blink::WebTextCheckingResult>* results) {
  if (text == "Helllo wordl.") {
    results->push_back(blink::WebTextCheckingResult(
        blink::kWebTextDecorationTypeSpelling, 0, 6,
        std::vector<blink::WebString>({"Hello"})));
    results->push_back(blink::WebTextCheckingResult(
        blink::kWebTextDecorationTypeSpelling, 7, 5,
        std::vector<blink::WebString>({"world"})));
    return true;
  }
  return false;
}

void WebTestSpellChecker::FillSuggestionList(
    const blink::WebString& word,
    std::vector<blink::WebString>* suggestions) {
  if (word == "wellcome")
    Append(suggestions, blink::WebString::FromUTF8("welcome"));
  else if (word == "upper case")
    Append(suggestions, blink::WebString::FromUTF8("uppercase"));
  else if (word == "Helllo")
    Append(suggestions, blink::WebString::FromUTF8("Hello"));
  else if (word == "wordl")
    Append(suggestions, blink::WebString::FromUTF8("world"));
}

}  // namespace content
