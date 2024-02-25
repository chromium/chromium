// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/parser/atomic_html_token.h"

namespace blink {

bool AtomicHTMLToken::UsesName() const {
  return type_ == HTMLToken::kStartTag || type_ == HTMLToken::kEndTag ||
         type_ == HTMLToken::DOCTYPE;
}

bool AtomicHTMLToken::UsesAttributes() const {
  return type_ == HTMLToken::kStartTag || type_ == HTMLToken::kEndTag;
}

#ifndef NDEBUG
const char* ToString(HTMLToken::TokenType type) {
  switch (type) {
#define DEFINE_STRINGIFY(type) \
  case HTMLToken::type:        \
    return #type;
    DEFINE_STRINGIFY(kUninitialized);
    DEFINE_STRINGIFY(DOCTYPE);
    DEFINE_STRINGIFY(kStartTag);
    DEFINE_STRINGIFY(kEndTag);
    DEFINE_STRINGIFY(kComment);
    DEFINE_STRINGIFY(kCharacter);
    DEFINE_STRINGIFY(kEndOfFile);
    DEFINE_STRINGIFY(kDOMPart);
#undef DEFINE_STRINGIFY
  }
  return "<unknown>";
}

void AtomicHTMLToken::Show() const {
  printf("AtomicHTMLToken %s", ToString(type_));
  switch (type_) {
    case HTMLToken::kStartTag:
    case HTMLToken::kEndTag:
      if (self_closing_)
        printf(" selfclosing");
      [[fallthrough]];
    case HTMLToken::DOCTYPE:
      printf(" name \"%s\"", GetName().GetString().Utf8().c_str());
      break;
    case HTMLToken::kComment:
    case HTMLToken::kCharacter:
      printf(" data \"%s\"", data_.Utf8().c_str());
      break;
    default:
      break;
  }
  // TODO(kouhei): print attributes_?
  printf("\n");
}
#endif

}  // namespace blink
