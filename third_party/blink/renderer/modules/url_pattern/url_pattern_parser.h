// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_URL_PATTERN_URL_PATTERN_PARSER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_URL_PATTERN_URL_PATTERN_PARSER_H_

#include <vector>

#include "base/types/strong_alias.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace liburlpattern {
struct Token;
}  // namespace liburlpattern

namespace blink {

class ExceptionState;
class URLPatternInit;

namespace url_pattern {

class Component;

class Parser final {
  STACK_ALLOCATED();

 public:
  explicit Parser(const String& input);

  // Attempt to parse the the input string used to construct the Parser object.
  // This method may only be called once.  Any errors will be thrown on the
  // give `exception_state`.  Retrieve the parse result by calling
  // `GetResult()`.  A protocol component will also be eagerly compiled for
  // absolute pattern strings.  It is not compiled for relative pattern string.
  // The compiled protocol Component can be accessed by calling
  // `GetProtocolComponent()`.
  void Parse(ExceptionState& exception_state);

  // Return the parse result.  Should only be called after `Parse()` succeeds.
  URLPatternInit* GetResult() const { return result_; }

  // Return the protocol component if it was compiled as part of parsing the
  // input string.  This should only be called after `Parse()` succeeds.
  // This will return nullptr if the input was a relative pattern string.
  Component* GetProtocolComponent() const { return protocol_component_; }

 private:
  enum class StringParseState {
    kProtocol,
    kHostname,
    kPort,
    kPathname,
    kSearch,
    kHash,
    kDone,
  };

  using Skip = base::StrongAlias<class SkipTag, int>;

  // A utility function to move from the current `state_` to `new_state`.  This
  // method will populate the component string in `result_` corresponding to the
  // current `state_` automatically.  It will also set `component_start_` and
  // `token_index_` to point to the first token of the next section based on how
  // many tokens the `skip` argument indicates should be ignored.
  void ChangeState(StringParseState new_state, Skip skip);

  // Attempt to access the Token at the given `index`.  If the `index` is out
  // of bounds for the `token_list_`, then the last Token in the list is
  // returned.  This will always be a `TokenType::kEnd` token.
  const liburlpattern::Token& SafeToken(size_t index) const;

  // Returns true if the token at the given `index` is not a special pattern
  // character and if it matches the given `value`.  This simply checks that the
  // token type is kChar, kEscapedChar, or kInvalidChar.
  bool IsNonSpecialPatternChar(size_t index, const char* value) const;

  // Returns true if the token at the given `index` is the protocol component
  // suffix; e.g. ':'.
  bool IsProtocolSuffix(size_t index) const;

  // Returns true if the next two tokens are slashes; e.g. `//`.
  bool NextIsAuthoritySlashes() const;

  // Returns true if the current token is the port prefix; e.g. `:`.
  bool IsPortPrefix() const;

  // Returns true if the current token is the start of the pathname; e.g. `/`.
  bool IsPathnameStart() const;

  // Returns true if the current token is the search component prefix; e.g. `?`.
  // This also takes into account if this could be a valid pattern modifier by
  // looking at the preceding tokens.
  bool IsSearchPrefix() const;

  // Returns true if the current token is the hsah component prefix; e.g. `#`.
  bool IsHashPrefix() const;

  // These methods indicate if the current token is opening or closing a pattern
  // grouping; e.g. `{` or `}`.
  bool IsGroupOpen() const;
  bool IsGroupClose() const;

  // This method returns a String consisting of the tokens between
  // `component_start_` and the current `token_index_`.
  String MakeComponentString() const;

  // Returns true if this URL should be treated as a "standard URL".  These URLs
  // automatically append a `/` for the pathname if one is not specified.
  void ComputeShouldTreatAsStandardURL(ExceptionState& exception_state);

  const String input_;
  const StringUTF8Adaptor utf8_;
  URLPatternInit* result_ = nullptr;
  Component* protocol_component_ = nullptr;
  std::vector<liburlpattern::Token> token_list_;
  size_t component_start_ = 0;
  size_t token_index_ = 0;
  StringParseState state_ = StringParseState::kPathname;
  bool in_group_ = false;
  bool should_treat_as_standard_url_ = false;
};

}  // namespace url_pattern
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_URL_PATTERN_URL_PATTERN_PARSER_H_
