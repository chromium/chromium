// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/url_pattern/url_pattern_parser.h"

#include "base/notreached.h"
#include "third_party/blink/renderer/modules/url_pattern/url_pattern_component.h"
#include "third_party/blink/renderer/modules/url_pattern/url_pattern_init.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "third_party/liburlpattern/tokenize.h"

namespace blink {
namespace url_pattern {

Parser::Parser(const String& input) : input_(input), utf8_(input) {}

void Parser::Parse(ExceptionState& exception_state) {
  auto tokenize_result =
      liburlpattern::Tokenize(absl::string_view(utf8_.data(), utf8_.size()),
                              liburlpattern::TokenizePolicy::kLenient);
  if (!tokenize_result.ok()) {
    // This should not happen with kLenient mode, but we handle it anyway.
    exception_state.ThrowTypeError("Invalid input string '" + input_ +
                                   "'. It unexpectedly fails to tokenize.");
    return;
  }

  token_list_ = std::move(tokenize_result.value());
  result_ = MakeGarbageCollected<URLPatternInit>();

  // We start in relative mode by default.  If we find a protocol `:` later,
  // we will update the starting state to expect an absolute URL pattern.
  DCHECK_EQ(state_, StringParseState::kPathname);

  // When constructing a pattern using structured input like
  // `new URLPattern({ pathname: 'foo' })` any missing components will be
  // defaulted to wildcards.  In this case, however, we default any missing
  // components to the empty string.  This is due to there being no way to
  // simply "leave out" a component when writing a URL.  The behavior also
  // matches the URL constructor.
  //
  // To that end we initialize components that would be set for a relative
  // pattern to the empty string default.  We don't do this for other
  // components right now so that any base URL value can set those components.
  result_->setPathname(g_empty_string);
  result_->setSearch(g_empty_string);
  result_->setHash(g_empty_string);

  // Scan for protocol `:` terminator.  This should be an invalid pattern
  // character.  This automatically works for "https://" because a name
  // cannot start with a `/`.  For URLs that do not include "//", however,
  // the input string will need to escape the colon, e.g. "data\\:foo".
  for (size_t i = 0; i < token_list_.size(); ++i) {
    if (IsProtocolSuffix(i)) {
      // Update the state to expect the start of an absolute URL.
      state_ = StringParseState::kProtocol;

      // Now that we are in absolute mode we know values will not be inherited
      // from a base URL.  Therefore initialize the rest of the components to
      // the empty string.
      result_->setProtocol(g_empty_string);
      result_->setUsername(g_empty_string);
      result_->setPassword(g_empty_string);
      result_->setHostname(g_empty_string);
      result_->setPort(g_empty_string);
      break;
    }
  }

  // Iterate through the list of tokens and update our state machine as we go.
  for (token_index_ = 0; token_index_ < token_list_.size(); ++token_index_) {
    // All states must respect the end of the token list.  The liburlpattern
    // tokenizer guarantees that the last token will have the type `kEnd`.
    if (token_list_[token_index_].type == liburlpattern::TokenType::kEnd) {
      ChangeState(StringParseState::kDone, Skip(0));
      break;
    }

    // In addition, all states must handle pattern groups.  We do not permit
    // a component to end in the middle of a pattern group.  Therefore we skip
    // past any tokens that are within `{` and `}`.  Note, the tokenizer
    // handles grouping `(` and `)` and `:foo` groups for us automatically, so
    // we don't need special code for them here.
    if (in_group_) {
      if (IsGroupClose())
        in_group_ = false;
      else
        continue;
    }

    if (IsGroupOpen()) {
      in_group_ = true;
      continue;
    }

    switch (state_) {
      case StringParseState::kProtocol: {
        // If we find the end of the protocol component...
        if (IsProtocolSuffix(token_index_)) {
          // First we eagerly compile the protocol pattern and use it to
          // compute if this entire URLPattern should be treated as a
          // "standard" URL.  If any of the special schemes, like `https`,
          // match the protocol pattern then we treat it as standard.  This
          // also forces the default pathname to be `/` instead of the empty
          // string.
          ComputeShouldTreatAsStandardURL(exception_state);
          if (exception_state.HadException())
            return;

          // Next, if there are authority slashes, like `https://`, then
          // we must transition to the authority section of the URLPattern.
          // We explicitly don't support username and password here.  We
          // instead go straight to hostname.
          if (NextIsAuthoritySlashes())
            ChangeState(StringParseState::kHostname, Skip(3));

          // If there are no authority slashes, but the protocol is special
          // then we still go to the hostname as this is a "standard" URL.
          // This differs from the above case since we don't need to skip the
          // extra slashes.
          else if (should_treat_as_standard_url_)
            ChangeState(StringParseState::kHostname, Skip(1));

          // Otherwise we treat this as a "cannot-be-a-base-URL" or what chrome
          // calls a "path" URL.  In this case we go straight to the pathname
          // component.  The hostname and port are left with their default
          // empty string values.
          else
            ChangeState(StringParseState::kPathname, Skip(1));
        }
        break;
      }

      case StringParseState::kHostname:
        // If we find a `:` then we transition to the port component state.
        if (IsPortPrefix())
          ChangeState(StringParseState::kPort, Skip(1));

        // If we find a `/` then we transition to the pathname component state.
        else if (IsPathnameStart())
          ChangeState(StringParseState::kPathname, Skip(0));

        // If we find a `?` then we transition to the search component state.
        else if (IsSearchPrefix())
          ChangeState(StringParseState::kSearch, Skip(1));

        // If we find a `#` then we transition to the hash component state.
        else if (IsHashPrefix())
          ChangeState(StringParseState::kHash, Skip(1));
        break;

      case StringParseState::kPort:
        // If we find a `/` then we transition to the pathname component state.
        if (IsPathnameStart())
          ChangeState(StringParseState::kPathname, Skip(0));
        // If we find a `?` then we transition to the search component state.
        else if (IsSearchPrefix())
          ChangeState(StringParseState::kSearch, Skip(1));
        // If we find a `#` then we transition to the hash component state.
        else if (IsHashPrefix())
          ChangeState(StringParseState::kHash, Skip(1));
        break;
      case StringParseState::kPathname:
        // If we find a `?` then we transition to the search component state.
        if (IsSearchPrefix())
          ChangeState(StringParseState::kSearch, Skip(1));
        // If we find a `#` then we transition to the hash component state.
        else if (IsHashPrefix())
          ChangeState(StringParseState::kHash, Skip(1));
        break;
      case StringParseState::kSearch:
        // If we find a `#` then we transition to the hash component state.
        if (IsHashPrefix())
          ChangeState(StringParseState::kHash, Skip(1));
        break;
      case StringParseState::kHash:
        // Nothing to do here as we are just looking for the end.
        break;
      case StringParseState::kDone:
        NOTREACHED();
        break;
    };
  }
}

void Parser::ChangeState(StringParseState new_state, Skip skip) {
  // First we convert the tokens between `component_start_` and `token_index_`
  // a component pattern string.  This is stored in the appropriate result
  // property based on the current `state_`.
  switch (state_) {
    case StringParseState::kProtocol: {
      result_->setProtocol(MakeComponentString());
      break;
    }
    case StringParseState::kHostname:
      result_->setHostname(MakeComponentString());
      break;
    case StringParseState::kPort:
      result_->setPort(MakeComponentString());
      break;
    case StringParseState::kPathname:
      result_->setPathname(MakeComponentString());
      break;
    case StringParseState::kSearch:
      result_->setSearch(MakeComponentString());
      break;
    case StringParseState::kHash:
      result_->setHash(MakeComponentString());
      break;
    case StringParseState::kDone:
      NOTREACHED();
      break;
  }

  // Next move to the new state.
  state_ = new_state;

  // Now update `component_start_` to point to the new component.  The `skip`
  // argument tells us how many tokens to ignore to get to the next start.
  component_start_ = SafeToken(token_index_ + skip.value()).index;

  // Next, move the `token_index_` so that the top of the loop will begin
  // parsing the new component.  The index will be automatically incremented by
  // the parse loop, so we move one less than the indicated `skip` amount.  This
  // means `kNone` and `kOne` are equivalent for setting `token_index_`.  Note,
  // however, these enums do have a different effect on setting
  // `component_start_` above.
  if (skip.value() > 1)
    token_index_ += (skip.value() - 1);
}

const liburlpattern::Token& Parser::SafeToken(size_t index) const {
  if (index < token_list_.size())
    return token_list_[index];
  DCHECK(!token_list_.empty());
  DCHECK(token_list_.back().type == liburlpattern::TokenType::kEnd);
  return token_list_.back();
}

bool Parser::IsNonSpecialPatternChar(size_t index, const char* value) const {
  const liburlpattern::Token& token = SafeToken(index);
  return token.value == value &&
         (token.type == liburlpattern::TokenType::kChar ||
          token.type == liburlpattern::TokenType::kEscapedChar ||
          token.type == liburlpattern::TokenType::kInvalidChar);
}

bool Parser::IsProtocolSuffix(size_t index) const {
  return IsNonSpecialPatternChar(index, ":");
}

bool Parser::NextIsAuthoritySlashes() const {
  return IsNonSpecialPatternChar(token_index_ + 1, "/") &&
         IsNonSpecialPatternChar(token_index_ + 2, "/");
}

bool Parser::IsPortPrefix() const {
  return IsNonSpecialPatternChar(token_index_, ":");
}

bool Parser::IsPathnameStart() const {
  return IsNonSpecialPatternChar(token_index_, "/");
}

bool Parser::IsSearchPrefix() const {
  if (IsNonSpecialPatternChar(token_index_, "?"))
    return true;

  if (token_list_[token_index_].value != "?")
    return false;

  // If we have a "?" that is not a normal character, then it must be an
  // optional group modifier.
  DCHECK_EQ(SafeToken(token_index_).type,
            liburlpattern::TokenType::kOtherModifier);

  // We have a `?` tokenized as a modifier.  We only want to treat this as
  // the search prefix if it would not normally be valid in a liburlpattern
  // string.  A modifier must follow a matching group.  Therefore we inspect
  // the preceding token to see if the `?` is immediately following a group
  // construct.
  //
  // So if the string is:
  //
  //  https://example.com/foo?bar
  //
  // Then we return true because the previous token is a `o` with type kChar.
  // For the string:
  //
  //  https://example.com/:name?bar
  //
  // Then we return false because the previous token is `:name` with type
  // kName.  If the developer intended this to be a search prefix then they
  // would need to escape like question mark like `:name\\?bar`.
  //
  // Note, if `token_index_` is zero the index will wrap around and
  // `SafeToken()` will return the kEnd token.  This will correctly return true
  // from this method as a pattern cannot normally begin with an unescaped `?`.
  const auto& previous_token = SafeToken(token_index_ - 1);
  return previous_token.type != liburlpattern::TokenType::kName &&
         previous_token.type != liburlpattern::TokenType::kRegex &&
         previous_token.type != liburlpattern::TokenType::kClose &&
         previous_token.type != liburlpattern::TokenType::kAsterisk;
}

bool Parser::IsHashPrefix() const {
  return IsNonSpecialPatternChar(token_index_, "#");
}

bool Parser::IsGroupOpen() const {
  return token_list_[token_index_].type == liburlpattern::TokenType::kOpen;
}

bool Parser::IsGroupClose() const {
  return token_list_[token_index_].type == liburlpattern::TokenType::kClose;
}

String Parser::MakeComponentString() const {
  DCHECK_LT(token_index_, token_list_.size());
  const auto& token = token_list_[token_index_];

  DCHECK_LE(component_start_, utf8_.size());
  DCHECK_GE(token.index, component_start_);
  DCHECK(token.index < utf8_.size() ||
         (token.index == utf8_.size() &&
          token.type == liburlpattern::TokenType::kEnd));

  return String::FromUTF8(utf8_.data() + component_start_,
                          token.index - component_start_);
}

void Parser::ComputeShouldTreatAsStandardURL(ExceptionState& exception_state) {
  DCHECK_EQ(state_, StringParseState::kProtocol);
  protocol_component_ =
      Component::Compile(MakeComponentString(), Component::Type::kProtocol,
                         /*protocol_component=*/nullptr, exception_state);
  if (protocol_component_ && protocol_component_->ShouldTreatAsStandardURL()) {
    should_treat_as_standard_url_ = true;
    result_->setPathname("/");
  }
}

}  // namespace url_pattern
}  // namespace blink
