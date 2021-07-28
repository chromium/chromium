// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_URL_PATTERN_URL_PATTERN_COMPONENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_URL_PATTERN_URL_PATTERN_COMPONENT_H_

#include "base/types/pass_key.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/bindings/core/v8/script_regexp.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/trace_traits.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/liburlpattern/parse.h"
#include "third_party/liburlpattern/pattern.h"

namespace blink {

class ExceptionState;

namespace url_pattern {

// A struct representing all the information needed to match a particular
// component of a URL.
class Component final : public GarbageCollected<Component> {
 public:
  // Enumeration defining the different types of components.  Each component
  // type uses a slightly different kind of character encoding.  In addition,
  // different component types using different liburlpattern parse options.
  enum class Type {
    kProtocol,
    kUsername,
    kPassword,
    kHostname,
    kPort,
    kPathname,
    kSearch,
    kHash,
  };

  // A utility function that takes a given `pattern` and compiles it into a
  // Component structure.  If the `pattern` is null then nullptr
  // may be returned without throwing an exception.  In this case the
  // Component is not constructed and the nullptr value should be
  // treated as matching any input value for the component.  The `type`
  // specifies which URL component is the pattern is being compiled for.  This
  // will select the correct encoding callback, liburlpattern options, and
  // populate errors messages with the correct component string.
  static Component* Compile(const String& pattern,
                            Type type,
                            Component* protocol_component,
                            ExceptionState& exception_state);

  // Compare the pattern strings in the two given components.  This provides a
  // mostly lexicographical ordering based on fixed text in the patterns.
  // Matching groups and modifiers are treated such that more restrictive
  // patterns are greater in value.  Group names are not considered in the
  // comparison.
  static int Compare(const Component& lh, const Component& rh);

  // Constructs a Component with a real `pattern` that compiled to the given
  // `regexp`.
  Component(Type type,
            liburlpattern::Pattern pattern,
            ScriptRegexp* regexp,
            Vector<String> name_list,
            base::PassKey<Component> key);

  // Constructs an empty Component that matches any input as if it had the
  // pattern `*`.
  Component(Type type, base::PassKey<Component> key);

  // Match the given `input` against the component pattern.  Returns `true`
  // if there is a match.  If `group_list` is not nullptr, then it will be
  // populated with group values captured by the pattern.
  bool Match(StringView input, Vector<String>* group_list) const;

  // Convert the compiled component pattern back into a pattern string.  This
  // will be functionally equivalent to the original, but may differ based on
  // canonicalization that occurred during parsing.
  String GeneratePatternString() const;

  // Combines the given list of group values with the group names specified in
  // the original pattern.  The return result is a vector of name:value tuples.
  Vector<std::pair<String, String>> MakeGroupList(
      const Vector<String>& group_values) const;

  // Method to determine if the URL associated with this component should be
  // treated as a "standard" URL like `https://foo` vs a "path" URL like
  // `data:foo`.  This should only be called for kProtocol components.
  bool ShouldTreatAsStandardURL() const;

  void Trace(Visitor* visitor) const;

 private:
  const Type type_;

  // The parsed pattern.
  const absl::optional<liburlpattern::Pattern> pattern_;

  // The pattern compiled down to a js regular expression.
  const Member<ScriptRegexp> regexp_;

  // The names to be applied to the regular expression capture groups.  Note,
  // liburlpattern regular expressions do not use named capture groups directly.
  const Vector<String> name_list_;
};

}  // namespace url_pattern
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_URL_PATTERN_URL_PATTERN_COMPONENT_H_
