// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WEB_TEST_RENDERER_WEB_TEST_GRAMMAR_CHECKER_H_
#define CONTENT_WEB_TEST_RENDERER_WEB_TEST_GRAMMAR_CHECKER_H_

#include <vector>

namespace blink {
class WebString;
struct WebTextCheckingResult;
}  // namespace blink

namespace content {

// A grammar-checker used for web tests. This class only implements the minimal
// functionarities required by web tests, i.e. this class just compares the
// given string with known grammar mistakes in web tests and adds grammar
// markers on them. Even though this is sufficient for web tests, this class is
// not suitable for any other usages.
class WebTestGrammarChecker {
 public:
  static bool CheckGrammarOfString(const blink::WebString&,
                                   std::vector<blink::WebTextCheckingResult>*);
};

}  // namespace content

#endif  // CONTENT_WEB_TEST_RENDERER_WEB_TEST_GRAMMAR_CHECKER_H_
