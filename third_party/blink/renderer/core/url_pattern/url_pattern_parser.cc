// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/url_pattern/url_pattern_parser.h"

#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_url_pattern_init.h"
#include "third_party/blink/renderer/core/url_pattern/url_pattern_component.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "third_party/liburlpattern/tokenize.h"

namespace blink {
namespace url_pattern {

Parser::Parser(const String& input, const URLPatternOptions& external_options)
    : input_(input), utf8_(input), external_options_(external_options) {}

void Parser::Parse(v8::Isolate* isolate, ExceptionState& exception_state) {
  DCHECK_EQ(state_, StringParseState::kInit);
  DCHECK_EQ(token_index_, 0u);

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

  // This enables the behavior proposed in WICG/urlpattern#179.
  const bool more_wildcards =
      RuntimeEnabledFeatures::URLPatternWildcardMoreOftenEnabled();

  // When constructing a pattern using structured input like
  // `new URLPattern({ pathname: 'foo' })` any missing components will be
  // defaulted to wildcards.
  //
  // If |more_wildcards| is false, then the string format will make every
  // component empty or a longer value, rather than a wildcard. This isn't done
  // immediately, so that the base URL can be included to provide some component
  // values for relative URLs. Therefore these values are initialized to their
  // default values only when the parser exits the kInit state, and it is known
  // if the pattern is relative or absolute.
  //
  // If |more_wildcards| is true, components which ordinarily appear "later"
  // than those specified are instead treated as wildcards, which avoids the
  // need to explicitly wildcard each of them. As a result, these values are not
  // initialized to be empty until a "later" component is seen.

  // Iterate through the list of tokens and update our state machine as we go.
  for (; token_index_ < token_list_.size(); token_index_ += token_increment_) {
    // Reset back to our default `token_increment_` value.
    token_increment_ = 1;

    // All states must respect the end of the token list.  The liburlpattern
    // tokenizer guarantees that the last token will have the type `kEnd`.
    if (token_list_[token_index_].type == liburlpattern::TokenType::kEnd) {
      // If we failed to find a protocol terminator then we are still in
      // relative mode.  We now need to determine the first component of the
      // relative URL.
      if (state_ == StringParseState::kInit) {
        // Reset back to the start of the input string.
        Rewind();

        // If the string begins with `?` then its a relative search component.
        // If it starts with `#` then its a relative hash component.  Otherwise
        // its a relative pathname.
        //
        // In each case we initialize any components following the initial
        // component to be empty string.
        if (IsHashPrefix()) {
          ChangeState(StringParseState::kHash, Skip(1));
        } else if (IsSearchPrefix()) {
          ChangeState(StringParseState::kSearch, Skip(1));
          if (!more_wildcards) {
            result_->setHash(g_empty_string);
          }
        } else {
          ChangeState(StringParseState::kPathname, Skip(0));
          if (!more_wildcards) {
            result_->setSearch(g_empty_string);
            result_->setHash(g_empty_string);
          }
        }
        continue;
      }

      // If we failed to find an `@`, then there is no username and password.
      // We should rewind and process the data as a hostname.
      else if (state_ == StringParseState::kAuthority) {
        RewindAndSetState(StringParseState::kHostname);
        continue;
      }

      ChangeState(StringParseState::kDone, Skip(0));
      break;
    }

    // In addition, all states must handle pattern groups.  We do not permit
    // a component to end in the middle of a pattern group.  Therefore we skip
    // past any tokens that are within `{` and `}`.  Note, the tokenizer
    // handles grouping `(` and `)` and `:foo` groups for us automatically, so
    // we don't need special code for them here.
    if (IsGroupOpen()) {
      group_depth_ += 1;
      continue;
    }

    if (group_depth_ > 0) {
      if (IsGroupClose())
        group_depth_ -= 1;
      else
        continue;
    }

    switch (state_) {
      case StringParseState::kInit:
        if (IsProtocolSuffix()) {
          if (!more_wildcards) {
            // We are in absolute mode and we know values will not be inherited
            // from a base URL.  Therefore initialize the rest of the components
            // to the empty string.
            result_->setUsername(g_empty_string);
            result_->setPassword(g_empty_string);
            result_->setHostname(g_empty_string);
            result_->setPort(g_empty_string);
            result_->setPathname(g_empty_string);
            result_->setSearch(g_empty_string);
            result_->setHash(g_empty_string);
          }

          // Update the state to expect the start of an absolute URL.
          RewindAndSetState(StringParseState::kProtocol);
        }
        break;

      case StringParseState::kProtocol:
        // If we find the end of the protocol component...
        if (IsProtocolSuffix()) {
          // First we eagerly compile the protocol pattern and use it to
          // compute if this entire URLPattern should be treated as a
          // "standard" URL.  If any of the special schemes, like `https`,
          // match the protocol pattern then we treat it as standard.
          ComputeShouldTreatAsStandardURL(isolate, exception_state);
          if (exception_state.HadException())
            return;

          // Standard URLs default to `/` for the pathname.
          //
          // If |more_wildcards| is true, we wait until actually seeing the
          // pathname or a later component to apply this default.
          if (should_treat_as_standard_url_ && !more_wildcards) {
            result_->setPathname("/");
          }

          // By default we treat this as a "cannot-be-a-base-URL" or what chrome
          // calls a "path" URL.  In this case we go straight to the pathname
          // component.  The hostname and port are left with their default
          // empty string values.
          StringParseState next_state = StringParseState::kPathname;
          Skip skip = Skip(1);

          // If there are authority slashes, like `https://`, then
          // we must transition to the authority section of the URLPattern.
          if (NextIsAuthoritySlashes()) {
            next_state = StringParseState::kAuthority;
            skip = Skip(3);
          }

          // If there are no authority slashes, but the protocol is special
          // then we still go to the authority section as this is a "standard"
          // URL.  This differs from the above case since we don't need to skip
          // the extra slashes.
          else if (should_treat_as_standard_url_) {
            next_state = StringParseState::kAuthority;
          }

          ChangeState(next_state, skip);
        }
        break;

      case StringParseState::kAuthority:
        // Before going to the hostname state we must see if there is an
        // identity of the form:
        //
        //  <username>:<password>@<hostname>
        //
        // We check for this by looking for the `@` character.  The username
        // and password are themselves each optional, so the `:` may not be
        // present.  If we see the `@` we just go to the username state
        // and let it proceed until it hits either the password separator
        // or the `@` terminator.
        if (IsIdentityTerminator())
          RewindAndSetState(StringParseState::kUsername);

        // Stop searching for the `@` character if we see the beginning
        // of the pathname, search, or hash components.
        else if (IsPathnameStart() || IsSearchPrefix() || IsHashPrefix())
          RewindAndSetState(StringParseState::kHostname);
        break;

      case StringParseState::kUsername:
        // If we find a `:` then transition to the password component state.
        if (IsPasswordPrefix())
          ChangeState(StringParseState::kPassword, Skip(1));

        // If we find a `@` then transition to the hostname component state.
        else if (IsIdentityTerminator())
          ChangeState(StringParseState::kHostname, Skip(1));
        break;

      case StringParseState::kPassword:
        // If we find a `@` then transition to the hostname component state.
        if (IsIdentityTerminator())
          ChangeState(StringParseState::kHostname, Skip(1));
        break;

      case StringParseState::kHostname:
        // Track whether we are inside ipv6 address brackets.
        if (IsIPv6Open())
          hostname_ipv6_bracket_depth_ += 1;
        else if (IsIPv6Close())
          hostname_ipv6_bracket_depth_ -= 1;

        // If we find a `:` then we transition to the port component state.
        // However, we ignore `:` when parsing an ipv6 address.
        else if (IsPortPrefix() && !hostname_ipv6_bracket_depth_)
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

  // Special case: if you specify a hostname, it is assumed that you want the
  // default port, if you didn't specify. This is ensures that
  // https://example.com/* does not match https://example.com:8443/, which is
  // another origin entirely.
  if (more_wildcards && result_->hasHostname() && !result_->hasPort()) {
    result_->setPort(g_empty_string);
  }
}

void Parser::ChangeState(StringParseState new_state, Skip skip) {
  // First we convert the tokens between `component_start_` and `token_index_`
  // a component pattern string.  This is stored in the appropriate result
  // property based on the current `state_`.
  switch (state_) {
    case StringParseState::kInit:
      // No component to set when transitioning from this state.
      break;
    case StringParseState::kProtocol:
      result_->setProtocol(MakeComponentString());
      present_components_.Put(Component::Type::kProtocol);
      break;
    case StringParseState::kAuthority:
      // No component to set when transitioning from this state.
      break;
    case StringParseState::kUsername:
      result_->setUsername(MakeComponentString());
      present_components_.Put(Component::Type::kUsername);
      break;
    case StringParseState::kPassword:
      result_->setPassword(MakeComponentString());
      present_components_.Put(Component::Type::kPassword);
      break;
    case StringParseState::kHostname:
      result_->setHostname(MakeComponentString());
      present_components_.Put(Component::Type::kHostname);
      break;
    case StringParseState::kPort:
      result_->setPort(MakeComponentString());
      present_components_.Put(Component::Type::kPort);
      break;
    case StringParseState::kPathname:
      result_->setPathname(MakeComponentString());
      present_components_.Put(Component::Type::kPathname);
      break;
    case StringParseState::kSearch:
      result_->setSearch(MakeComponentString());
      present_components_.Put(Component::Type::kSearch);
      break;
    case StringParseState::kHash:
      result_->setHash(MakeComponentString());
      present_components_.Put(Component::Type::kHash);
      break;
    case StringParseState::kDone:
      NOTREACHED();
      break;
  }

  if (RuntimeEnabledFeatures::URLPatternWildcardMoreOftenEnabled() &&
      state_ != StringParseState::kInit &&
      new_state != StringParseState::kDone) {
    // If a component was skipped but a later component is present, it gets its
    // default value, explicitly.
    //
    // This relies on the ordering of the states, which does correspond to the
    // order of components (aside from authority/username/password, which are
    // special).
    static_assert(
        base::ranges::is_sorted(std::initializer_list<StringParseState>{
            StringParseState::kHostname, StringParseState::kPort,
            StringParseState::kPathname, StringParseState::kSearch,
            StringParseState::kHash}));
    if (state_ < StringParseState::kHostname &&
        new_state > StringParseState::kHostname && !result_->hasHostname()) {
      result_->setHostname(g_empty_string);
    }
    if (state_ < StringParseState::kPort &&
        new_state > StringParseState::kPort && !result_->hasPort()) {
      result_->setPort(g_empty_string);
    }
    if (state_ < StringParseState::kPathname &&
        new_state > StringParseState::kPathname && !result_->hasPathname()) {
      result_->setPathname(should_treat_as_standard_url_ ? "/"
                                                         : g_empty_string);
    }
    if (state_ < StringParseState::kSearch &&
        new_state > StringParseState::kSearch && !result_->hasSearch()) {
      result_->setSearch(g_empty_string);
    }
  }

  ChangeStateWithoutSettingComponent(new_state, skip);
}

void Parser::ChangeStateWithoutSettingComponent(StringParseState new_state,
                                                Skip skip) {
  state_ = new_state;

  // Now update `component_start_` to point to the new component.  The `skip`
  // argument tells us how many tokens to ignore to get to the next start.
  component_start_ = token_index_ + skip.value();

  // Next, move the `token_index_` so that the top of the loop will begin
  // parsing the new component.  We adjust the `token_increment_` down to
  // zero as the skip value already takes into account moving to the start
  // of the next component.
  token_index_ += skip.value();
  token_increment_ = 0;
}

void Parser::Rewind() {
  token_index_ = component_start_;
  token_increment_ = 0;
}

void Parser::RewindAndSetState(StringParseState new_state) {
  Rewind();
  state_ = new_state;
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

bool Parser::IsProtocolSuffix() const {
  return IsNonSpecialPatternChar(token_index_, ":");
}

bool Parser::NextIsAuthoritySlashes() const {
  return IsNonSpecialPatternChar(token_index_ + 1, "/") &&
         IsNonSpecialPatternChar(token_index_ + 2, "/");
}

bool Parser::IsIdentityTerminator() const {
  return IsNonSpecialPatternChar(token_index_, "@");
}

bool Parser::IsPasswordPrefix() const {
  return IsNonSpecialPatternChar(token_index_, ":");
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

bool Parser::IsIPv6Open() const {
  return IsNonSpecialPatternChar(token_index_, "[");
}

bool Parser::IsIPv6Close() const {
  return IsNonSpecialPatternChar(token_index_, "]");
}

String Parser::MakeComponentString() const {
  DCHECK_LT(token_index_, token_list_.size());
  const auto& token = token_list_[token_index_];

  size_t component_char_start = SafeToken(component_start_).index;

  DCHECK_LE(component_char_start, utf8_.size());
  DCHECK_GE(token.index, component_char_start);
  DCHECK(token.index < utf8_.size() ||
         (token.index == utf8_.size() &&
          token.type == liburlpattern::TokenType::kEnd));

  return String::FromUTF8(utf8_.data() + component_char_start,
                          token.index - component_char_start);
}

void Parser::ComputeShouldTreatAsStandardURL(v8::Isolate* isolate,
                                             ExceptionState& exception_state) {
  DCHECK_EQ(state_, StringParseState::kProtocol);
  protocol_component_ = Component::Compile(
      isolate, MakeComponentString(), Component::Type::kProtocol,
      /*protocol_component=*/nullptr, external_options_, exception_state);
  if (protocol_component_ && protocol_component_->ShouldTreatAsStandardURL())
    should_treat_as_standard_url_ = true;
}

}  // namespace url_pattern
}  // namespace blink
