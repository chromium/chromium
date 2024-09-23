// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/web_test/renderer/web_test_spell_checker.h"

#include <stddef.h>

#include <algorithm>

#include "base/check_op.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"

namespace content {

namespace {

void Append(blink::WebVector<blink::WebString>* data,
            const blink::WebString& item) {
  blink::WebVector<blink::WebString> result(data->size() + 1);
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

  // Initialize this spellchecker.
  InitializeIfNeeded();

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
        base::ranges::find_if(string_text, IsASCIIAlpha);
    if (first_char == string_text.end())
      return true;
    int word_offset = std::distance(string_text.begin(), first_char);
    int max_word_length = static_cast<int>(string_text.length()) - word_offset;
    int word_length;
    std::u16string word;

    // Look up our misspelled-word table to check if the extracted word is a
    // known misspelled word, and return the offset and the length of the
    // extracted word if this word is a known misspelled word.
    // (See the comment in WebTestSpellChecker::InitializeIfNeeded() why we use
    // a misspelled-word table.)
    for (size_t i = 0; i < misspelled_words_.size(); ++i) {
      word_length =
          static_cast<int>(misspelled_words_.at(i).length()) > max_word_length
              ? max_word_length
              : static_cast<int>(misspelled_words_.at(i).length());
      word = string_text.substr(word_offset, word_length);
      if (word == misspelled_words_.at(i) &&
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
    blink::WebVector<blink::WebString>* suggestions) {
  if (word == "wellcome")
    Append(suggestions, blink::WebString::FromUTF8("welcome"));
  else if (word == "upper case")
    Append(suggestions, blink::WebString::FromUTF8("uppercase"));
  else if (word == "Helllo")
    Append(suggestions, blink::WebString::FromUTF8("Hello"));
  else if (word == "wordl")
    Append(suggestions, blink::WebString::FromUTF8("world"));
}

bool WebTestSpellChecker::InitializeIfNeeded() {
  // Exit if we have already initialized this object.
  if (initialized_)
    return false;

  // Create a table that consists of misspelled words used in Blink web tests.
  // Since Blink web tests don't have so many misspelled words as
  // well-spelled words, it is easier to compare the given word with misspelled
  // ones than to compare with well-spelled ones.
  static const char* misspelled_words[] = {
      // These words are known misspelled words in web tests.
      // If there are other misspelled words in web tests, please add them in
      // this array.
      "foo", "Foo", "baz", "fo", "LibertyF", "chello", "xxxtestxxx", "XXxxx",
      "Textx", "blockquoted", "asd", "Lorem", "Nunc", "Curabitur", "eu", "adlj",
      "adaasj", "sdklj", "jlkds", "jsaada", "jlda", "contentEditable",
      // Prefer to match the full word than a partial word when there's an
      // ambiguous boundary.
      "zz't", "zz",
      // The following words are used by unit tests.
      "ifmmp", "qwertyuiopasd", "qwertyuiopasdf", "upper case", "wellcome"};

  misspelled_words_.clear();
  for (size_t i = 0; i < std::size(misspelled_words); ++i)
    misspelled_words_.push_back(
        std::u16string(misspelled_words[i],
                       misspelled_words[i] + strlen(misspelled_words[i])));

  // Mark as initialized to prevent this object from being initialized twice
  // or more.
  initialized_ = true;

  // Since this WebTestSpellChecker class doesn't download dictionaries, this
  // function always returns false.
  return false;
}

}  // namespace content
