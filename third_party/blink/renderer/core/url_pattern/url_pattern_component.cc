// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/url_pattern/url_pattern_component.h"

#include <string_view>

#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "components/url_pattern/url_pattern_util.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_url_pattern_options.h"
#include "third_party/blink/renderer/core/url_pattern/url_pattern_canon.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "url/url_util.h"

namespace blink {
namespace url_pattern {

namespace {

// Utility method to convert a type to a string.
StringView TypeToString(Component::Type type) {
  switch (type) {
    case Component::Type::kProtocol:
      return "protocol";
    case Component::Type::kUsername:
      return "username";
    case Component::Type::kPassword:
      return "password";
    case Component::Type::kHostname:
      return "hostname";
    case Component::Type::kPort:
      return "port";
    case Component::Type::kPathname:
      return "pathname";
    case Component::Type::kSearch:
      return "search";
    case Component::Type::kHash:
      return "hash";
  }
  NOTREACHED_IN_MIGRATION();
}

// Utility method to get the correct encoding callback for a given type.
liburlpattern::EncodeCallback GetEncodeCallback(std::string_view pattern_utf8,
                                                Component::Type type,
                                                Component* protocol_component) {
  switch (type) {
    case Component::Type::kProtocol:
      return ::url_pattern::ProtocolEncodeCallback;
    case Component::Type::kUsername:
      return ::url_pattern::UsernameEncodeCallback;
    case Component::Type::kPassword:
      return ::url_pattern::PasswordEncodeCallback;
    case Component::Type::kHostname:
      if (::url_pattern::TreatAsIPv6Hostname(pattern_utf8)) {
        return ::url_pattern::IPv6HostnameEncodeCallback;
      } else {
        return ::url_pattern::HostnameEncodeCallback;
      }
    case Component::Type::kPort:
      return ::url_pattern::PortEncodeCallback;
    case Component::Type::kPathname:
      // Different types of URLs use different canonicalization for pathname.
      // A "standard" URL flattens `.`/`..` and performs full percent encoding.
      // A "path" URL does not flatten and uses a more lax percent encoding.
      // The spec calls "path" URLs as "cannot-be-a-base-URL" URLs:
      //
      //  https://url.spec.whatwg.org/#cannot-be-a-base-url-path-state
      //
      // We prefer "standard" URL here by checking to see if the protocol
      // pattern matches any of the known standard protocol strings.  So
      // an exact pattern of `http` will match, but so will `http{s}?` and
      // `*`.
      //
      // If the protocol pattern does not match any of the known standard URL
      // protocols then we fall back to the "path" URL behavior.  This will
      // normally be triggered by `data`, `javascript`, `about`, etc.  It
      // will also be triggered for custom protocol strings.  We favor "path"
      // behavior here because its better to under canonicalize since the
      // developer can always manually canonicalize the pathname for a custom
      // protocol.
      //
      // ShouldTreatAsStandardURL can by a bit expensive, so only do it if we
      // actually have a pathname pattern to compile.
      CHECK(protocol_component);
      if (protocol_component->ShouldTreatAsStandardURL())
        return ::url_pattern::StandardURLPathnameEncodeCallback;
      else
        return ::url_pattern::PathURLPathnameEncodeCallback;
    case Component::Type::kSearch:
      return ::url_pattern::SearchEncodeCallback;
    case Component::Type::kHash:
      return ::url_pattern::HashEncodeCallback;
  }
  NOTREACHED_IN_MIGRATION();
}

// Utility method to get the correct liburlpattern parse options for a given
// type.
const liburlpattern::Options GetOptions(
    Component::Type type,
    Component* protocol_component,
    const URLPatternOptions& external_options) {
  using liburlpattern::Options;

  // The liburlpattern::Options to use for most component patterns.  We
  // default to strict mode and most components have no concept of a delimiter
  // or prefix character.  Case sensitivity is set via the external options.
  Options value = {.delimiter_list = "",
                   .prefix_list = "",
                   .sensitive = !external_options.ignoreCase(),
                   .strict = true};

  if (type == Component::Type::kHostname) {
    // Hostname patterns use a "." delimiter controlling how far a named group
    // like ":bar" will match.
    value.delimiter_list = ".";

  } else if (type == Component::Type::kPathname) {
    // Just like how we select a different encoding callback based on
    // whether we are treating the pattern string as a standard or
    // cannot-be-a-base URL, we must also choose the right liburlppatern
    // options as well.  We should only use use the options that treat
    // `/` specially if we are treating this a standard URL.
    DCHECK(protocol_component);
    if (protocol_component->ShouldTreatAsStandardURL()) {
      // Pathname patterns for "standard" URLs use a "/" delimiter controlling
      // how far a named group like ":bar" will match.  They also use "/" as an
      // automatic prefix before groups.
      value.delimiter_list = "/";
      value.prefix_list = "/";
    }
  }

  return value;
}

int ComparePart(const liburlpattern::Part& lh, const liburlpattern::Part& rh) {
  // We prioritize PartType in the ordering so we can favor fixed text.  The
  // type ordering is:
  //
  //  kFixed > kRegex > kSegmentWildcard > kFullWildcard.
  //
  // We considered kRegex greater than the wildcards because it is likely to be
  // used for imposing some constraint and not just duplicating wildcard
  // behavior.
  //
  // This comparison depends on the PartType enum in liburlpattern having the
  // correct corresponding numeric values.
  //
  // Next the Modifier is considered:
  //
  //  kNone > kOneOrMore > kOptional > kZeroOrMore.
  //
  // The rationale here is that requring the match group to exist is more
  // restrictive then making it optional and requiring an exact count is more
  // restrictive than repeating.
  //
  // This comparison depends on the Modifier enum in liburlpattern having the
  // correct corresponding numeric values.
  //
  // Finally we lexicographically compare the text components from left to
  // right; `prefix`, `value`, and `suffix`.  Its ok to depend on simple
  // byte-wise string comparison here because the values have all been URL
  // encoded.  This guarantees the strings contain only ASCII.
  auto left = std::tie(lh.type, lh.modifier, lh.prefix, lh.value, lh.suffix);
  auto right = std::tie(rh.type, rh.modifier, rh.prefix, rh.value, rh.suffix);
  if (left < right)
    return -1;
  else if (left == right)
    return 0;
  else
    return 1;
}

}  // anonymous namespace

// static
Component* Component::Compile(v8::Isolate* isolate,
                              StringView pattern,
                              Type type,
                              Component* protocol_component,
                              const URLPatternOptions& external_options,
                              ExceptionState& exception_state) {
  StringView final_pattern = pattern.IsNull() ? "*" : pattern;
  const liburlpattern::Options& options =
      GetOptions(type, protocol_component, external_options);

  // Parse the pattern.
  // Lossy UTF8 conversion is fine given the input has come through a
  // USVString webidl argument.
  StringUTF8Adaptor utf8(final_pattern);
  auto parse_result = liburlpattern::Parse(
      utf8.AsStringView(),
      GetEncodeCallback(utf8.AsStringView(), type, protocol_component),
      options);
  if (!parse_result.ok()) {
    exception_state.ThrowTypeError(
        "Invalid " + TypeToString(type) + " pattern '" + final_pattern + "'. " +
        String::FromUTF8(parse_result.status().message()));
    return nullptr;
  }

  Vector<String> wtf_name_list;
  ScriptRegexp* regexp = nullptr;

  if (!parse_result.value().CanDirectMatch()) {
    // Extract a regular expression string from the parsed pattern.
    std::vector<std::string> name_list;
    std::string regexp_string =
        parse_result.value().GenerateRegexString(&name_list);

    // Compile the regular expression to verify it is valid.
    auto case_sensitive = options.sensitive ? WTF::kTextCaseSensitive
                                            : WTF::kTextCaseASCIIInsensitive;
    DCHECK(base::IsStringASCII(regexp_string));
    regexp = MakeGarbageCollected<ScriptRegexp>(
        isolate, String(regexp_string.data(), regexp_string.size()),
        case_sensitive, MultilineMode::kMultilineDisabled,
        UnicodeMode::kUnicodeSets);

    if (!regexp->IsValid()) {
      // The regular expression failed to compile.  This means that some
      // custom regexp group within the pattern is illegal.  Attempt to
      // compile each regexp group individually in order to identify the
      // culprit.
      for (auto& part : parse_result.value().PartList()) {
        if (part.type != liburlpattern::PartType::kRegex)
          continue;
        DCHECK(base::IsStringASCII(part.value));
        String group_value(part.value.data(), part.value.size());
        regexp = MakeGarbageCollected<ScriptRegexp>(
            isolate, group_value, case_sensitive,
            MultilineMode::kMultilineDisabled, UnicodeMode::kUnicodeSets);
        if (regexp->IsValid())
          continue;
        exception_state.ThrowTypeError("Invalid " + TypeToString(type) +
                                       " pattern '" + final_pattern +
                                       "'. Custom regular expression group '" +
                                       group_value + "' is invalid.");
        return nullptr;
      }
      // We couldn't find a bad regexp group, but we still have an overall
      // error.  This shouldn't happen, but we handle it anyway.
      exception_state.ThrowTypeError("Invalid " + TypeToString(type) +
                                     " pattern '" + final_pattern +
                                     "'. An unexpected error has occurred.");
      return nullptr;
    }

    wtf_name_list.ReserveInitialCapacity(
        static_cast<wtf_size_t>(name_list.size()));
    for (const auto& name : name_list) {
      wtf_name_list.push_back(String::FromUTF8(name));
    }
  }

  return MakeGarbageCollected<Component>(
      type, std::move(parse_result.value()), std::move(regexp),
      std::move(wtf_name_list), base::PassKey<Component>());
}

// static
int Component::Compare(const Component& lh, const Component& rh) {
  using liburlpattern::Modifier;
  using liburlpattern::Part;
  using liburlpattern::PartType;

  auto& left = lh.pattern_.PartList();
  auto& right = rh.pattern_.PartList();

  // Begin by comparing each Part in the lists with each other.  If any
  // are not equal, then we are done.
  size_t i = 0;
  for (; i < left.size() && i < right.size(); ++i) {
    int r = ComparePart(left[i], right[i]);
    if (r)
      return r;
  }

  // We reached the end of at least one of the lists without finding a
  // difference.  However, we must handle the case where one list is longer
  // than the other.  In this case we compare the next Part from the
  // longer list to a synthetically created empty kFixed Part.  This is
  // necessary in order for "/foo/" to be considered more restrictive, and
  // therefore greater, than "/foo/*".
  if (i == left.size() && i != right.size())
    return ComparePart(Part(PartType::kFixed, "", Modifier::kNone), right[i]);
  else if (i != left.size() && i == right.size())
    return ComparePart(left[i], Part(PartType::kFixed, "", Modifier::kNone));

  // No differences were found, so declare them equal.
  return 0;
}

Component::Component(Type type,
                     liburlpattern::Pattern pattern,
                     ScriptRegexp* regexp,
                     Vector<String> name_list,
                     base::PassKey<Component> key)
    : type_(type),
      pattern_(std::move(pattern)),
      regexp_(regexp),
      name_list_(std::move(name_list)) {}

bool Component::Match(StringView input,
                      Vector<std::pair<String, String>>* group_list) const {
  // If there is a regexp, then we cannot directly match the pattern.
  if (regexp_) {
    Vector<String> value_list;
    if (group_list) {
      value_list.ReserveInitialCapacity(
          base::checked_cast<wtf_size_t>(name_list_.size()));
    }
    bool result =
        regexp_->Match(input, /*start_from=*/0, /*match_length=*/nullptr,
                       group_list ? &value_list : nullptr) == 0;
    if (result && group_list) {
      DCHECK_EQ(name_list_.size(), value_list.size());
      group_list->ReserveInitialCapacity(name_list_.size());
      for (wtf_size_t i = 0; i < name_list_.size(); ++i) {
        group_list->emplace_back(name_list_[i], std::move(value_list[i]));
      }
    }
    return result;
  }

  // There is no regexp, so directly match against the pattern.
  std::vector<std::pair<std::string_view, std::optional<std::string_view>>>
      pattern_group_list;
  // Lossy UTF8 conversion is fine given the input has come through a
  // USVString webidl argument.
  StringUTF8Adaptor utf8(input);
  bool result = pattern_.DirectMatch(
      utf8.AsStringView(), group_list ? &pattern_group_list : nullptr);
  if (group_list) {
    group_list->ReserveInitialCapacity(
        base::checked_cast<wtf_size_t>(pattern_group_list.size()));
    for (const auto& pair : pattern_group_list) {
      // We need to be careful converting the group value to a WTF::String.
      // If the value is std::nullopt, then we want to use a null String.
      // If the value exists, but is zero length, then we want to use an empty
      // string.  We must handle this explicitly since FromUTF8() can convert
      // some zero length strings to null String.
      String value;
      if (pair.second.has_value()) {
        if (pair.second->empty()) {
          value = g_empty_string;
        } else {
          value = String::FromUTF8(*pair.second);
        }
      }
      group_list->emplace_back(String::FromUTF8(pair.first), std::move(value));
    }
  }
  return result;
}

String Component::GeneratePatternString() const {
  return String::FromUTF8(pattern_.GeneratePatternString());
}

bool Component::ShouldTreatAsStandardURL() const {
  DCHECK(type_ == Type::kProtocol);

  if (should_treat_as_standard_url_.has_value())
    return *should_treat_as_standard_url_;

  const auto protocol_matches = [&](const std::string& scheme) {
    DCHECK(base::IsStringASCII(scheme));
    return Match(String(scheme.data(), static_cast<unsigned>(scheme.size())),
                 /*group_list=*/nullptr);
  };

  should_treat_as_standard_url_ =
      base::ranges::any_of(url::GetStandardSchemes(), protocol_matches);
  return *should_treat_as_standard_url_;
}

const std::vector<liburlpattern::Part>& Component::PartList() const {
  return pattern_.PartList();
}

void Component::Trace(Visitor* visitor) const {
  visitor->Trace(regexp_);
}

}  // namespace url_pattern
}  // namespace blink
