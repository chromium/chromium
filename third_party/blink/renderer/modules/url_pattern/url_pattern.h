// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_URL_PATTERN_URL_PATTERN_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_URL_PATTERN_URL_PATTERN_H_

#include "base/types/pass_key.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace liburlpattern {
struct Options;
}  // namespace liburlpattern

namespace blink {

class ExceptionState;
class URLPatternComponentResult;
class URLPatternInit;
class URLPatternResult;
class USVStringOrURLPatternInit;

class URLPattern : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();
  class Component;

 public:
  static URLPattern* Create(const URLPatternInit* init,
                            ExceptionState& exception_state);

  URLPattern(Component* protocol,
             Component* username,
             Component* password,
             Component* hostname,
             Component* port,
             Component* pathname,
             Component* search,
             Component* hash,
             base::PassKey<URLPattern> key);

  bool test(const USVStringOrURLPatternInit& input,
            ExceptionState& exception_state) const;
  URLPatternResult* exec(const USVStringOrURLPatternInit& input,
                         ExceptionState& exception_state) const;

  // TODO: define a stringifier

  void Trace(Visitor* visitor) const override;

 private:
  // A utility function that takes a given |pattern| and compiles it into a
  // Component structure.  If the |pattern| matches the given |default_pattern|
  // then nullptr may be returned without throwing an exception.  In this case
  // the Component is not constructed and the nullptr value should be treated as
  // matching any input value for the component.  The |component| string is used
  // for exception messages.  The |options| control how the pattern is compiled.
  static Component* CompilePattern(const String& pattern,
                                   StringView component,
                                   const liburlpattern::Options& options,
                                   ExceptionState& exception_state);

  // A utility function to determine if a given |input| matches the pattern
  // or not.  Returns |true| if there is a match and |false| otherwise.  If
  // |result| is not nullptr then the URLPatternResult contents will be filled
  // in as expected by the exec() method.
  bool Match(const USVStringOrURLPatternInit& input,
             URLPatternResult* result,
             ExceptionState& exception_state) const;

  // A utility function that constructs a URLPatternComponentResult for
  // a given |component|, |input|, and |group_list|.  The |component| may
  // be nullptr.
  static URLPatternComponentResult* MakeComponentResult(
      Component* component,
      const String& input,
      const Vector<String>& group_list);

  // The compiled patterns for each URL component.  If a Component member is
  // nullptr then it should be treated as a wildcard matching any input.
  Member<Component> protocol_;
  Member<Component> username_;
  Member<Component> password_;
  Member<Component> hostname_;
  Member<Component> port_;
  Member<Component> pathname_;
  Member<Component> search_;
  Member<Component> hash_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_URL_PATTERN_URL_PATTERN_H_
