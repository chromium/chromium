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
class URLPatternInit;
class URLPatternResult;
class USVStringOrURLPatternInit;

class URLPattern : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();
  struct Component;

 public:
  static URLPattern* Create(const URLPatternInit* init,
                            ExceptionState& exception_state);

  URLPattern(std::unique_ptr<Component> protocol,
             std::unique_ptr<Component> username,
             std::unique_ptr<Component> password,
             std::unique_ptr<Component> hostname,
             std::unique_ptr<Component> port,
             std::unique_ptr<Component> pathname,
             std::unique_ptr<Component> search,
             std::unique_ptr<Component> hash,
             base::PassKey<URLPattern> key);

  bool test(const USVStringOrURLPatternInit& input,
            ExceptionState& exception_state);
  URLPatternResult* exec(const USVStringOrURLPatternInit& input,
                         ExceptionState& exception_state);
  String toRegExp(const String& component, ExceptionState& exception_state);

  // TODO: define a stringifier

 private:
  // A utility function that takes a given |pattern| and compiles it into a
  // Component structure.  If the |pattern| matches the given |default_pattern|
  // then nullptr may be returned without throwing an exception.  In this case
  // the Component is not constructed and the nullptr value should be treated as
  // matching any input value for the component.  The |component| string is used
  // for exception messages.  The |options| control how the pattern is compiled.
  static std::unique_ptr<Component> CompilePattern(
      const String& pattern,
      const String& default_pattern,
      StringView component,
      const liburlpattern::Options& options,
      ExceptionState& exception_state);

  // The compiled patterns for each URL component.  If a Component member is
  // nullptr then it should be treated as a wildcard matching any input.
  std::unique_ptr<Component> protocol_;
  std::unique_ptr<Component> username_;
  std::unique_ptr<Component> password_;
  std::unique_ptr<Component> hostname_;
  std::unique_ptr<Component> port_;
  std::unique_ptr<Component> pathname_;
  std::unique_ptr<Component> search_;
  std::unique_ptr<Component> hash_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_URL_PATTERN_URL_PATTERN_H_
