// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_URL_PATTERN_URL_PATTERN_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_URL_PATTERN_URL_PATTERN_H_

#include "base/types/pass_key.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_typedefs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_url_pattern_component.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/liburlpattern/parse.h"

namespace blink {

class ExceptionState;
class URLPatternComponentResult;
class URLPatternInit;
class URLPatternResult;

namespace url_pattern {
class Component;
}  // namespace url_pattern

class MODULES_EXPORT URLPattern : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();
  using Component = url_pattern::Component;

 public:
  static URLPattern* Create(const V8URLPatternInput* input,
                            const String& base_url,
                            ExceptionState& exception_state);

  static URLPattern* Create(const V8URLPatternInput* input,
                            ExceptionState& exception_state);

  static URLPattern* Create(const URLPatternInit* init,
                            Component* precomputed_protocol_component,
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

  bool test(const V8URLPatternInput* input,
            const String& base_url,
            ExceptionState& exception_state) const;
  bool test(const V8URLPatternInput* input,
            ExceptionState& exception_state) const;

  URLPatternResult* exec(const V8URLPatternInput* input,
                         const String& base_url,
                         ExceptionState& exception_state) const;
  URLPatternResult* exec(const V8URLPatternInput* input,
                         ExceptionState& exception_state) const;

  String protocol() const;
  String username() const;
  String password() const;
  String hostname() const;
  String port() const;
  String pathname() const;
  String search() const;
  String hash() const;

  static int compareComponent(const V8URLPatternComponent& component,
                              const URLPattern* left,
                              const URLPattern* right);

  void Trace(Visitor* visitor) const override;

 private:
  // A utility function to determine if a given |input| matches the pattern
  // or not.  Returns |true| if there is a match and |false| otherwise.  If
  // |result| is not nullptr then the URLPatternResult contents will be filled.
  bool Match(const V8URLPatternInput* input,
             const String& base_url,
             URLPatternResult* result,
             ExceptionState& exception_state) const;

  // A utility function that constructs a URLPatternComponentResult for
  // a given |component|, |input|, and |group_list|.
  static URLPatternComponentResult* MakeURLPatternComponentResult(
      Component* component,
      const String& input,
      const Vector<String>& group_values);

  // The compiled patterns for each URL component.
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
