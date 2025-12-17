// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_URL_PATTERN_URL_PATTERN_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_URL_PATTERN_URL_PATTERN_H_

#include <array>
#include <utility>

#include "base/types/pass_key.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_typedefs.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_url_pattern_component.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/url_pattern/url_pattern_component.h"
#include "third_party/blink/renderer/core/url_pattern/url_pattern_options.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/liburlpattern/parse.h"

namespace blink {

class ExceptionState;
class KURL;
struct SafeUrlPattern;
class URLPatternInit;
class URLPatternOptions;
class URLPatternResult;

class CORE_EXPORT URLPattern : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

  using Options = url_pattern::Options;
  using Component = url_pattern::Component;

 public:
  // Used to convert the convenience types that may be passed to WebIDL APIs in
  // place of a URLPattern into a URLPattern object. `base_url` will usually be
  // the result of calling `ExecutionContext::BaseURL`.
  static URLPattern* From(v8::Isolate* isolate,
                          const V8URLPatternCompatible* compatible,
                          const KURL& base_url,
                          ExceptionState& exception_state);

  static URLPattern* Create(v8::Isolate* isolate,
                            const V8URLPatternInput* input,
                            const String& base_url,
                            const URLPatternOptions* options,
                            ExceptionState& exception_state);

  static URLPattern* Create(v8::Isolate* isolate,
                            const V8URLPatternInput* input,
                            const String& base_url,
                            ExceptionState& exception_state);

  static URLPattern* Create(v8::Isolate* isolate,
                            const V8URLPatternInput* input,
                            const URLPatternOptions* options,
                            ExceptionState& exception_state);

  static URLPattern* Create(v8::Isolate* isolate,
                            const V8URLPatternInput* input,
                            ExceptionState& exception_state);

  static URLPattern* Create(v8::Isolate* isolate,
                            const URLPatternInit* init,
                            Component* precomputed_protocol_component,
                            const URLPatternOptions* options,
                            ExceptionState& exception_state);

  URLPattern(Component* protocol,
             Component* username,
             Component* password,
             Component* hostname,
             Component* port,
             Component* pathname,
             Component* search,
             Component* hash,
             const Options& options,
             base::PassKey<URLPattern> key);

  bool test(v8::Isolate*,
            const V8URLPatternInput* input,
            const String& base_url,
            ExceptionState& exception_state) const;
  bool test(v8::Isolate*,
            const V8URLPatternInput* input,
            ExceptionState& exception_state) const;

  URLPatternResult* exec(v8::Isolate*,
                         const V8URLPatternInput* input,
                         const String& base_url,
                         ExceptionState& exception_state) const;
  URLPatternResult* exec(v8::Isolate*,
                         const V8URLPatternInput* input,
                         ExceptionState& exception_state) const;

  String generate(const V8URLPatternComponent& component,
                  const VectorOfPairs<String, String>& groups,
                  ExceptionState& exception_state) const;

  String protocol() const;
  String username() const;
  String password() const;
  String hostname() const;
  String port() const;
  String pathname() const;
  String search() const;
  String hash() const;

  bool hasRegExpGroups() const;

  static int compareComponent(const V8URLPatternComponent& component,
                              const URLPattern* left,
                              const URLPattern* right);

  // Matched :group values. This is essentially a C++ version of
  // the URLPatternResult API (but without inputs).
  struct MatchResult {
    Vector<std::pair<String, String>> protocol;
    Vector<std::pair<String, String>> username;
    Vector<std::pair<String, String>> password;
    Vector<std::pair<String, String>> hostname;
    Vector<std::pair<String, String>> port;
    Vector<std::pair<String, String>> pathname;
    Vector<std::pair<String, String>> search;
    Vector<std::pair<String, String>> hash;
  };

  // Match the specified URL against this pattern. Return true if it's a match,
  // false otherwise. Populate `result` with matched :group values in each
  // component, if not nullptr.
  bool Match(const KURL& url, MatchResult* = nullptr) const;

  // Throws a TypeError if the pattern does not meet the requirements to be
  // safe. i.e. has no regexp groups.
  std::optional<SafeUrlPattern> ToSafeUrlPattern(
      ExceptionState& exception_state) const;

  // Used for testing and debugging.
  String ToString() const;

  void Trace(Visitor* visitor) const override;

 private:
  // A utility function to determine if a given `input` matches the pattern or
  // not.  Returns `true` if there is a match and `false` otherwise.  If
  // `result` is not nullptr then the URLPatternResult contents will be filled.
  bool Match(v8::Isolate*,
             const V8URLPatternInput* input,
             const String& base_url,
             URLPatternResult* result,
             ExceptionState& exception_state) const;

  // String representations of each component of a URL used to match against a
  // URLPattern.
  struct MatchInput {
    String protocol = g_empty_string;
    String username = g_empty_string;
    String password = g_empty_string;
    String hostname = g_empty_string;
    String port = g_empty_string;
    String pathname = g_empty_string;
    String search = g_empty_string;
    String hash = g_empty_string;
  };

  static void URLToMatchInput(const KURL&, MatchInput&);
  bool Match(const MatchInput&, MatchResult* = nullptr) const;

  std::array<std::pair<const Member<Component>&, const char*>, 8>
  ComponentsWithNames() const {
    return {{{protocol_, "protocol"},
             {username_, "username"},
             {password_, "password"},
             {hostname_, "hostname"},
             {port_, "port"},
             {pathname_, "pathname"},
             {search_, "search"},
             {hash_, "hash"}}};
  }

  bool ShouldTreatAsStandardURL() const {
    CHECK(protocol_);
    return protocol_->ShouldTreatAsStandardURL();
  }

  // The compiled patterns for each URL component.
  Member<Component> protocol_;
  Member<Component> username_;
  Member<Component> password_;
  Member<Component> hostname_;
  Member<Component> port_;
  Member<Component> pathname_;
  Member<Component> search_;
  Member<Component> hash_;
  const Options options_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_URL_PATTERN_URL_PATTERN_H_
