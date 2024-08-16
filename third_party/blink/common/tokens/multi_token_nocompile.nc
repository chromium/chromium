// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// https://dev.chromium.org/developers/testing/no-compile-tests

#include "third_party/blink/public/common/tokens/multi_token.h"

#include "base/types/token_type.h"

namespace blink {

using FooToken = base::TokenType<class FooTokenTag>;
using BarToken = base::TokenType<class BarTokenTag>;
using BazToken = base::TokenType<class BazTokenTag>;

void NoTokenType() {
  MultiToken<> token;  // expected-error {{constraints not satisfied for class template 'MultiToken' [with Tokens = <>]}}
}

void OneTokenType() {
  MultiToken<FooToken> token;  // expected-error {{constraints not satisfied for class template 'MultiToken' [with Tokens = <base::TokenType<blink::FooTokenTag>>]}}
}

void DuplicateTokenType() {
  MultiToken<FooToken, FooToken> token;  // expected-error {{constraints not satisfied for class template 'MultiToken' [with Tokens = <base::TokenType<blink::FooTokenTag>, base::TokenType<blink::FooTokenTag>>]}}
}

void NonCompatibleMultiTokenConstruction() {
  MultiToken<FooToken, BarToken, BazToken> foo_bar_baz_token;
  MultiToken<FooToken, BarToken> foo_bar_token(foo_bar_baz_token);  // expected-error {{no matching constructor for initialization of 'MultiToken<FooToken, BarToken>' (aka 'MultiToken<TokenType<class FooTokenTag>, TokenType<class BarTokenTag>>')}}
}

void NonCompatibleMultiTokenAssignment() {
  MultiToken<FooToken, BarToken, BazToken> foo_bar_baz_token;
  MultiToken<FooToken, BarToken> foo_bar_token;
  foo_bar_token = foo_bar_baz_token;  // expected-error {{no viable overloaded '='}}
}

}  // namespace blink
