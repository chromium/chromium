// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_ACCESSIBILITY_ANNOTATIONS_AX_IMAGE_STOPWORDS_H_
#define CONTENT_RENDERER_ACCESSIBILITY_ANNOTATIONS_AX_IMAGE_STOPWORDS_H_

#include <string_view>

#include "base/containers/flat_set.h"
#include "base/no_destructor.h"
#include "content/common/content_export.h"

namespace content {

// Maintains a set of image stopwords and provides a function to check
// whether or not a given word is an image stopword.
//
// A stopword in general is a word that's filtered out before doing
// natural language processing. In English, common stopwords include
// "the" or "of" - they are words that are part of grammatically correct
// sentences but don't add any useful semantics themselves.
//
// This set is used as part of an algorithm to determine whether the
// accessible label for an image (including the "alt" attribute and
// other attributes) contains a useful description or not. For this
// application, both common stopwords like "the", but also image-related
// words like "image" and "photo" are included, because an image that's
// just labeled with the word "photo" is essentially unlabeled.
//
// Stopwords from all supported languages are grouped together, because
// it's simpler to just have one set rather than to try to split by the
// element language (which is sometimes wrong). This leads to a small
// but acceptable number of false positives if a stopword in one language
// is a meaningful word in another language.
//
// The set of supported languages should include all of the languages
// that we can generate automatic image descriptions for. This will grow
// over time.
//
// Words consisting of just one or two characters made up of letters from
// Latin alphabets are always considered stopwords, but that doesn't
// generalize to all languages / character sets.
//
// The set of stopwords was obtained by extracting the alt text of images
// from billions of web pages, tokenizing, counting, and then manually
// categorizing the top words, with the help of dictionaries and language
// experts. More details in this (Google-internal) design doc:
// http://goto.google.com/augment-existing-image-descriptions
class CONTENT_EXPORT AXImageStopwords {
 public:
  static AXImageStopwords& GetInstance();

  // The input should be a word, after already splitting by punctuation and
  // whitespace. Returns true if the word is an image stopword.
  // Case-insensitive and language-neutral (includes words from all
  // languages).
  bool IsImageStopword(const char* utf8_string) const;

 private:
  friend base::NoDestructor<AXImageStopwords>;

  AXImageStopwords();
  ~AXImageStopwords();

  base::flat_set<std::string_view> stopword_set_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_ACCESSIBILITY_ANNOTATIONS_AX_IMAGE_STOPWORDS_H_
