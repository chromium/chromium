// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/url_pattern/url_pattern.h"

#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "third_party/blink/public/common/safe_url_pattern.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_urlpattern_urlpatterninit_usvstring.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_urlpatterninit_usvstring.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_url_pattern_component_result.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_url_pattern_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_url_pattern_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_url_pattern_result.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/url_pattern/url_pattern_canon.h"
#include "third_party/blink/renderer/core/url_pattern/url_pattern_component.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_regexp.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_to_number.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/liburlpattern/constructor_string_parser.h"
#include "third_party/liburlpattern/pattern.h"
#include "third_party/liburlpattern/tokenize.h"
#include "third_party/liburlpattern/utils.h"

namespace blink {

using url_pattern::Component;
using url_pattern::ValueType;
using ComponentSet = base::EnumSet<Component::Type,
                                   Component::Type::kProtocol,
                                   Component::Type::kHash>;

namespace {

// Utility function to determine if a pathname is absolute or not.  For
// kURL values this mainly consists of a check for a leading slash.  For
// patterns we do some additional checking for escaped or grouped slashes.
bool IsAbsolutePathname(const String& pathname, ValueType type) {
  if (pathname.empty())
    return false;

  if (pathname[0] == '/')
    return true;

  if (type == ValueType::kURL)
    return false;

  if (pathname.length() < 2)
    return false;

  // Patterns treat escaped slashes and slashes within an explicit grouping as
  // valid leading slashes.  For example, "\/foo" or "{/foo}".  Patterns do
  // not consider slashes within a custom regexp group as valid for the leading
  // pathname slash for now.  To support that we would need to be able to
  // detect things like ":name_123(/foo)" as a valid leading group in a pattern,
  // but that is considered too complex for now.
  if ((pathname[0] == '\\' || pathname[0] == '{') && pathname[1] == '/') {
    return true;
  }

  return false;
}

// Utility function to determine if the default port for the given protocol
// matches the given port number.
bool IsProtocolDefaultPort(const String& protocol, const String& port) {
  if (protocol.empty() || port.empty())
    return false;

  bool port_ok = false;
  int port_number = port.Impl()->ToInt(WTF::NumberParsingOptions(), &port_ok);
  if (!port_ok)
    return false;

  StringUTF8Adaptor protocol_utf8(protocol);
  int default_port = url::DefaultPortForScheme(protocol_utf8.AsStringView());
  return default_port != url::PORT_UNSPECIFIED && default_port == port_number;
}

// Base URL values that include pattern string characters should not blow
// up pattern parsing.  Automatically escape them.  We must not escape inputs
// for non-pattern base URLs, though.
String EscapeBaseURLString(const StringView& input, ValueType type) {
  if (input.empty()) {
    return g_empty_string;
  }

  if (type != ValueType::kPattern) {
    return input.ToString();
  }

  std::string result;
  result.reserve(input.length());

  StringUTF8Adaptor utf8(input);
  liburlpattern::EscapePatternStringAndAppend(utf8.AsStringView(), result);

  return String::FromUTF8(result);
}

// A utility method that takes a URLPatternInit, splits it apart, and applies
// the individual component values in the given set of strings.  The strings
// are only applied if a value is present in the init structure.
void ApplyInit(const URLPatternInit* init,
               ValueType type,
               String& protocol,
               String& username,
               String& password,
               String& hostname,
               String& port,
               String& pathname,
               String& search,
               String& hash,
               ExceptionState& exception_state) {
  // If there is a baseURL we need to apply its component values first.  The
  // rest of the URLPatternInit structure will then later override these
  // values.  Note, the baseURL will always set either an empty string or
  // longer value for each considered component.  We do not allow null strings
  // to persist for these components past this phase since they should no
  // longer be treated as wildcards.
  KURL base_url;
  if (init->hasBaseURL()) {
    base_url = KURL(init->baseURL());
    if (!base_url.IsValid() || base_url.IsEmpty()) {
      exception_state.ThrowTypeError("Invalid baseURL '" + init->baseURL() +
                                     "'.");
      return;
    }

    // Components are only inherited from the base URL if no "earlier" component
    // is specified in |init|.  Furthermore, when the base URL is being used as
    // the basis of a pattern (not a URL being matched against), usernames and
    // passwords are always wildcarded unless explicitly specified otherwise,
    // because they usually do not affect which resource is requested (though
    // they do often affect whether access is authorized).
    //
    // Even though they appear earlier than the hostname in a URL, the username
    // and password are treated as appearing after it because they typically
    // refer to credentials within a realm on an origin, rather than being used
    // across all hostnames.
    //
    // This partial ordering is represented by the following diagram:
    //
    //                                 +-> pathname --> search --> hash
    // protocol --> hostname --> port -|
    //                                 +-> username --> password
    protocol = init->hasProtocol()
                   ? String()
                   : EscapeBaseURLString(base_url.Protocol(), type);
    username = (type == ValueType::kPattern ||
                (init->hasProtocol() || init->hasHostname() ||
                 init->hasPort() || init->hasUsername()))
                   ? String()
                   : EscapeBaseURLString(base_url.User(), type);
    password = (type == ValueType::kPattern ||
                (init->hasProtocol() || init->hasHostname() ||
                 init->hasPort() || init->hasUsername() || init->hasPassword()))
                   ? String()
                   : EscapeBaseURLString(base_url.Pass(), type);
    hostname = (init->hasProtocol() || init->hasHostname())
                   ? String()
                   : EscapeBaseURLString(base_url.Host(), type);
    port = (init->hasProtocol() || init->hasHostname() || init->hasPort())
               ? String()
           : base_url.Port() > 0 ? String::Number(base_url.Port())
                                 : g_empty_string;
    pathname = (init->hasProtocol() || init->hasHostname() || init->hasPort() ||
                init->hasPathname())
                   ? String()
                   : EscapeBaseURLString(base_url.GetPath(), type);
    search = (init->hasProtocol() || init->hasHostname() || init->hasPort() ||
              init->hasPathname() || init->hasSearch())
                 ? String()
                 : EscapeBaseURLString(base_url.Query(), type);
    hash = (init->hasProtocol() || init->hasHostname() || init->hasPort() ||
            init->hasPathname() || init->hasSearch() || init->hasHash())
               ? String()
           : base_url.HasFragmentIdentifier()
               ? EscapeBaseURLString(base_url.FragmentIdentifier(), type)
               : g_empty_string;
  }

  // Apply the URLPatternInit component values on top of the default and
  // baseURL values.
  if (init->hasProtocol()) {
    protocol = url_pattern::CanonicalizeProtocol(init->protocol(), type,
                                                 exception_state);
    if (exception_state.HadException())
      return;
  }
  if (init->hasUsername() || init->hasPassword()) {
    String init_username = init->hasUsername() ? init->username() : String();
    String init_password = init->hasPassword() ? init->password() : String();
    url_pattern::CanonicalizeUsernameAndPassword(init_username, init_password,
                                                 type, username, password,
                                                 exception_state);
    if (exception_state.HadException())
      return;
  }
  if (init->hasHostname()) {
    hostname = url_pattern::CanonicalizeHostname(init->hostname(), type,
                                                 exception_state);
    if (exception_state.HadException())
      return;
  }
  if (init->hasPort()) {
    port = url_pattern::CanonicalizePort(init->port(), type, protocol,
                                         exception_state);
    if (exception_state.HadException())
      return;
  }
  if (init->hasPathname()) {
    pathname = init->pathname();
    if (base_url.IsValid() && base_url.IsHierarchical() &&
        !IsAbsolutePathname(pathname, type)) {
      // Find the last slash in the baseURL pathname.  Since the URL is
      // hierarchical it should have a slash to be valid, but we are cautious
      // and check.  If there is no slash then we cannot use resolve the
      // relative pathname and just treat the init pathname as an absolute
      // value.
      String base_path = EscapeBaseURLString(base_url.GetPath(), type);
      auto slash_index = base_path.ReverseFind("/");
      if (slash_index != kNotFound) {
        // Extract the baseURL path up to and including the first slash.  Append
        // the relative init pathname to it.
        pathname = base_path.Substring(0, slash_index + 1) + pathname;
      }
    }
    pathname = url_pattern::CanonicalizePathname(protocol, pathname, type,
                                                 exception_state);
    if (exception_state.HadException())
      return;
  }
  if (init->hasSearch()) {
    search =
        url_pattern::CanonicalizeSearch(init->search(), type, exception_state);
    if (exception_state.HadException())
      return;
  }
  if (init->hasHash()) {
    hash = url_pattern::CanonicalizeHash(init->hash(), type, exception_state);
    if (exception_state.HadException())
      return;
  }
}

URLPatternComponentResult* MakeURLPatternComponentResult(
    ScriptState* script_state,
    const String& input,
    const Vector<std::pair<String, String>>& group_values) {
  auto* result = URLPatternComponentResult::Create();
  result->setInput(input);

  // Convert null WTF::String values to v8::Undefined.  We have to do this
  // manually because the webidl compiler compiler does not currently
  // support `(USVString or undefined)` in a record value.
  // TODO(crbug.com/1293259): Use webidl `(USVString or undefined)` when
  //                          available in the webidl compiler.
  HeapVector<std::pair<String, ScriptValue>> v8_group_values;
  v8_group_values.reserve(group_values.size());
  for (const auto& pair : group_values) {
    v8::Local<v8::Value> v8_value;
    if (pair.second.IsNull()) {
      v8_value = v8::Undefined(script_state->GetIsolate());
    } else {
      v8_value = ToV8Traits<IDLUSVString>::ToV8(script_state, pair.second);
    }
    v8_group_values.emplace_back(
        pair.first,
        ScriptValue(script_state->GetIsolate(), std::move(v8_value)));
  }

  result->setGroups(std::move(v8_group_values));
  return result;
}

URLPatternInit* MakeURLPatternInit(
    const liburlpattern::ConstructorStringParser::Result& result) {
  auto* init = URLPatternInit::Create();
  if (result.protocol) {
    init->setProtocol(String::FromUTF8(*result.protocol));
  }
  if (result.username) {
    init->setUsername(String::FromUTF8(*result.username));
  }
  if (result.password) {
    init->setPassword(String::FromUTF8(*result.password));
  }
  if (result.hostname) {
    init->setHostname(String::FromUTF8(*result.hostname));
  }
  if (result.port) {
    init->setPort(String::FromUTF8(*result.port));
  }
  if (result.pathname) {
    init->setPathname(String::FromUTF8(*result.pathname));
  }
  if (result.search) {
    init->setSearch(String::FromUTF8(*result.search));
  }
  if (result.hash) {
    init->setHash(String::FromUTF8(*result.hash));
  }
  return init;
}

ComponentSet ToURLPatternComponentSet(
    const liburlpattern::ConstructorStringParser::ComponentSet&
        present_components) {
  ComponentSet result;
  if (present_components.protocol) {
    result.Put(Component::Type::kProtocol);
  }
  if (present_components.username) {
    result.Put(Component::Type::kUsername);
  }
  if (present_components.password) {
    result.Put(Component::Type::kPassword);
  }
  if (present_components.hostname) {
    result.Put(Component::Type::kHostname);
  }
  if (present_components.port) {
    result.Put(Component::Type::kPort);
  }
  if (present_components.pathname) {
    result.Put(Component::Type::kPathname);
  }
  if (present_components.search) {
    result.Put(Component::Type::kSearch);
  }
  if (present_components.hash) {
    result.Put(Component::Type::kHash);
  }
  return result;
}

}  // namespace

URLPattern* URLPattern::From(v8::Isolate* isolate,
                             const V8URLPatternCompatible* compatible,
                             const KURL& base_url,
                             ExceptionState& exception_state) {
  switch (compatible->GetContentType()) {
    case V8URLPatternCompatible::ContentType::kURLPattern:
      return compatible->GetAsURLPattern();
    case V8URLPatternCompatible::ContentType::kURLPatternInit: {
      URLPatternInit* original_init = compatible->GetAsURLPatternInit();
      URLPatternInit* init;
      if (original_init->hasBaseURL()) {
        init = original_init;
      } else {
        init = URLPatternInit::Create();
        if (original_init->hasProtocol()) {
          init->setProtocol(original_init->protocol());
        }
        if (original_init->hasUsername()) {
          init->setUsername(original_init->username());
        }
        if (original_init->hasPassword()) {
          init->setPassword(original_init->password());
        }
        if (original_init->hasHostname()) {
          init->setHostname(original_init->hostname());
        }
        if (original_init->hasPort()) {
          init->setPort(original_init->port());
        }
        if (original_init->hasPathname()) {
          init->setPathname(original_init->pathname());
        }
        if (original_init->hasSearch()) {
          init->setSearch(original_init->search());
        }
        if (original_init->hasHash()) {
          init->setHash(original_init->hash());
        }
        init->setBaseURL(base_url.GetString());
      }
      return Create(isolate, init, /*precomputed_protocol_component=*/nullptr,
                    MakeGarbageCollected<URLPatternOptions>(), exception_state);
    }
    case V8URLPatternCompatible::ContentType::kUSVString:
      return Create(
          isolate,
          MakeGarbageCollected<V8URLPatternInput>(compatible->GetAsUSVString()),
          base_url.GetString(), MakeGarbageCollected<URLPatternOptions>(),
          exception_state);
  }
}

URLPattern* URLPattern::Create(v8::Isolate* isolate,
                               const V8URLPatternInput* input,
                               const String& base_url,
                               const URLPatternOptions* options,
                               ExceptionState& exception_state) {
  if (input->GetContentType() ==
      V8URLPatternInput::ContentType::kURLPatternInit) {
    exception_state.ThrowTypeError(
        "Invalid second argument baseURL '" + base_url +
        "' provided with a URLPatternInit input. Use the "
        "URLPatternInit.baseURL property instead.");
    return nullptr;
  }

  const auto& input_string = input->GetAsUSVString();
  const StringUTF8Adaptor utf8_string(input_string);
  liburlpattern::ConstructorStringParser constructor_string_parser(
      utf8_string.AsStringView());

  Component* protocol_component = nullptr;
  absl::Status status = constructor_string_parser.Parse(
      [=, &protocol_component, &exception_state](
          std::string_view protocol_string) -> absl::StatusOr<bool> {
        protocol_component = Component::Compile(
            isolate, String::FromUTF8(protocol_string),
            Component::Type::kProtocol,
            /*protocol_component=*/nullptr, *options, exception_state);
        if (exception_state.HadException()) {
          return absl::InvalidArgumentError("Failed to compile protocol");
        }
        return protocol_component &&
               protocol_component->ShouldTreatAsStandardURL();
      });

  if (exception_state.HadException()) {
    return nullptr;
  }
  if (!status.ok()) {
    exception_state.ThrowTypeError("Invalid input string '" + input_string +
                                   "'. It unexpectedly fails to tokenize.");
    return nullptr;
  }
  URLPatternInit* init =
      MakeURLPatternInit(constructor_string_parser.GetResult());

  if (!base_url && !init->hasProtocol()) {
    exception_state.ThrowTypeError(
        "Relative constructor string '" + input_string +
        "' must have a base URL passed as the second argument.");
    return nullptr;
  }

  if (base_url)
    init->setBaseURL(base_url);

  URLPattern* result =
      Create(isolate, init, protocol_component, options, exception_state);
  if (result) {
    URLPattern::ComponentSet present = ToURLPatternComponentSet(
        constructor_string_parser.GetPresentComponents());
    URLPattern::ComponentSet wildcard_with_string_format_change =
        URLPattern::ComponentSet::All();
    wildcard_with_string_format_change.RemoveAll(present);
    if (present.Has(Component::Type::kUsername)) {
      wildcard_with_string_format_change.RemoveAll({Component::Type::kProtocol,
                                                    Component::Type::kHostname,
                                                    Component::Type::kPort});
    }
    if (present.Has(Component::Type::kPassword)) {
      wildcard_with_string_format_change.RemoveAll(
          {Component::Type::kProtocol, Component::Type::kHostname,
           Component::Type::kPort, Component::Type::kUsername});
    }
    if (present.Has(Component::Type::kHostname)) {
      // As a special case, don't wildcard the port if the host is present, even
      // with no path.
      wildcard_with_string_format_change.RemoveAll(
          {Component::Type::kProtocol, Component::Type::kPort});
    }
    if (present.Has(Component::Type::kPort)) {
      wildcard_with_string_format_change.RemoveAll(
          {Component::Type::kProtocol, Component::Type::kHostname});
    }
    if (present.Has(Component::Type::kPathname)) {
      wildcard_with_string_format_change.RemoveAll({Component::Type::kProtocol,
                                                    Component::Type::kHostname,
                                                    Component::Type::kPort});
    }
    if (present.Has(Component::Type::kSearch)) {
      wildcard_with_string_format_change.RemoveAll(
          {Component::Type::kProtocol, Component::Type::kHostname,
           Component::Type::kPort, Component::Type::kPathname});
    }
    if (present.Has(Component::Type::kHash)) {
      wildcard_with_string_format_change.RemoveAll(
          {Component::Type::kProtocol, Component::Type::kHostname,
           Component::Type::kPort, Component::Type::kPathname,
           Component::Type::kSearch});
    }
    result->wildcard_with_string_format_change_ =
        wildcard_with_string_format_change;
  }
  return result;
}

URLPattern* URLPattern::Create(v8::Isolate* isolate,
                               const V8URLPatternInput* input,
                               const String& base_url,
                               ExceptionState& exception_state) {
  return Create(isolate, input, base_url,
                MakeGarbageCollected<URLPatternOptions>(), exception_state);
}

URLPattern* URLPattern::Create(v8::Isolate* isolate,
                               const V8URLPatternInput* input,
                               const URLPatternOptions* options,
                               ExceptionState& exception_state) {
  if (input->IsURLPatternInit()) {
    return URLPattern::Create(isolate, input->GetAsURLPatternInit(),
                              /*precomputed_protocol_component=*/nullptr,
                              options, exception_state);
  }
  return Create(isolate, input, /*base_url=*/String(), options,
                exception_state);
}

URLPattern* URLPattern::Create(v8::Isolate* isolate,
                               const V8URLPatternInput* input,
                               ExceptionState& exception_state) {
  if (input->IsURLPatternInit()) {
    return URLPattern::Create(isolate, input->GetAsURLPatternInit(),
                              /*precomputed_protocol_component=*/nullptr,
                              MakeGarbageCollected<URLPatternOptions>(),
                              exception_state);
  }

  return Create(isolate, input, /*base_url=*/String(), exception_state);
}

URLPattern* URLPattern::Create(v8::Isolate* isolate,
                               const URLPatternInit* init,
                               Component* precomputed_protocol_component,
                               const URLPatternOptions* options,
                               ExceptionState& exception_state) {
  // Each component defaults to a wildcard matching any input.  We use
  // the null string as a shorthand for the default.
  String protocol;
  String username;
  String password;
  String hostname;
  String port;
  String pathname;
  String search;
  String hash;

  // Apply the input URLPatternInit on top of the default values.
  ApplyInit(init, ValueType::kPattern, protocol, username, password, hostname,
            port, pathname, search, hash, exception_state);
  if (exception_state.HadException())
    return nullptr;

  // Manually canonicalize port patterns that exactly match the default
  // port for the protocol.  We must do this separately from the compile
  // since the liburlpattern::Parse() method will invoke encoding callbacks
  // for partial values within the pattern and this transformation must apply
  // to the entire value.
  if (IsProtocolDefaultPort(protocol, port))
    port = "";

  // Compile each component pattern into a Component structure that
  // can be used for matching.

  auto* protocol_component = precomputed_protocol_component;
  if (!protocol_component) {
    protocol_component = Component::Compile(
        isolate, protocol, Component::Type::kProtocol,
        /*protocol_component=*/nullptr, *options, exception_state);
  }
  if (exception_state.HadException())
    return nullptr;

  auto* username_component =
      Component::Compile(isolate, username, Component::Type::kUsername,
                         protocol_component, *options, exception_state);
  if (exception_state.HadException())
    return nullptr;

  auto* password_component =
      Component::Compile(isolate, password, Component::Type::kPassword,
                         protocol_component, *options, exception_state);
  if (exception_state.HadException())
    return nullptr;

  auto* hostname_component =
      Component::Compile(isolate, hostname, Component::Type::kHostname,
                         protocol_component, *options, exception_state);
  if (exception_state.HadException())
    return nullptr;

  auto* port_component =
      Component::Compile(isolate, port, Component::Type::kPort,
                         protocol_component, *options, exception_state);
  if (exception_state.HadException())
    return nullptr;

  auto* pathname_component =
      Component::Compile(isolate, pathname, Component::Type::kPathname,
                         protocol_component, *options, exception_state);
  if (exception_state.HadException())
    return nullptr;

  auto* search_component =
      Component::Compile(isolate, search, Component::Type::kSearch,
                         protocol_component, *options, exception_state);
  if (exception_state.HadException())
    return nullptr;

  auto* hash_component =
      Component::Compile(isolate, hash, Component::Type::kHash,
                         protocol_component, *options, exception_state);
  if (exception_state.HadException())
    return nullptr;

  Options urlpattern_options;
  urlpattern_options.ignore_case = options->ignoreCase();

  URLPattern* result = MakeGarbageCollected<URLPattern>(
      protocol_component, username_component, password_component,
      hostname_component, port_component, pathname_component, search_component,
      hash_component, urlpattern_options, base::PassKey<URLPattern>());
  if (init->hasBaseURL()) {
    auto& would_be_wildcard = result->wildcard_with_base_url_change_;
    if (!init->hasUsername() &&
        (init->hasProtocol() || init->hasHostname() || init->hasPort())) {
      would_be_wildcard.Put(Component::Type::kUsername);
    }
    if (!init->hasPassword() && (init->hasProtocol() || init->hasHostname() ||
                                 init->hasPort() || init->hasUsername())) {
      would_be_wildcard.Put(Component::Type::kPassword);
    }
    if (!init->hasHostname() && init->hasProtocol()) {
      would_be_wildcard.Put(Component::Type::kHostname);
    }
    if (!init->hasPort() && (init->hasProtocol() || init->hasHostname())) {
      would_be_wildcard.Put(Component::Type::kPort);
    }
    if (!init->hasPathname() &&
        (init->hasProtocol() || init->hasHostname() || init->hasPort())) {
      would_be_wildcard.Put(Component::Type::kPathname);
    }
    if (!init->hasSearch() && (init->hasProtocol() || init->hasHostname() ||
                               init->hasPort() || init->hasPathname())) {
      would_be_wildcard.Put(Component::Type::kSearch);
    }
    if (!init->hasHash() &&
        (init->hasProtocol() || init->hasHostname() || init->hasPort() ||
         init->hasPathname() || init->hasSearch())) {
      would_be_wildcard.Put(Component::Type::kHash);
    }
  }
  return result;
}

URLPattern::URLPattern(Component* protocol,
                       Component* username,
                       Component* password,
                       Component* hostname,
                       Component* port,
                       Component* pathname,
                       Component* search,
                       Component* hash,
                       Options options,
                       base::PassKey<URLPattern> key)
    : protocol_(protocol),
      username_(username),
      password_(password),
      hostname_(hostname),
      port_(port),
      pathname_(pathname),
      search_(search),
      hash_(hash),
      options_(options) {}

bool URLPattern::test(ScriptState* script_state,
                      const V8URLPatternInput* input,
                      const String& base_url,
                      ExceptionState& exception_state) const {
  return Match(script_state, input, base_url, /*result=*/nullptr,
               exception_state);
}

bool URLPattern::test(ScriptState* script_state,
                      const V8URLPatternInput* input,
                      ExceptionState& exception_state) const {
  return test(script_state, input, /*base_url=*/String(), exception_state);
}

URLPatternResult* URLPattern::exec(ScriptState* script_state,
                                   const V8URLPatternInput* input,
                                   const String& base_url,
                                   ExceptionState& exception_state) const {
  URLPatternResult* result = URLPatternResult::Create();
  if (!Match(script_state, input, base_url, result, exception_state))
    return nullptr;
  return result;
}

URLPatternResult* URLPattern::exec(ScriptState* script_state,
                                   const V8URLPatternInput* input,
                                   ExceptionState& exception_state) const {
  return exec(script_state, input, /*base_url=*/String(), exception_state);
}

String URLPattern::protocol() const {
  return protocol_->GeneratePatternString();
}

String URLPattern::username() const {
  return username_->GeneratePatternString();
}

String URLPattern::password() const {
  return password_->GeneratePatternString();
}

String URLPattern::hostname() const {
  return hostname_->GeneratePatternString();
}

String URLPattern::port() const {
  return port_->GeneratePatternString();
}

String URLPattern::pathname() const {
  return pathname_->GeneratePatternString();
}

String URLPattern::search() const {
  return search_->GeneratePatternString();
}

String URLPattern::hash() const {
  return hash_->GeneratePatternString();
}

bool URLPattern::hasRegExpGroups() const {
  const url_pattern::Component* components[] = {protocol_, username_, password_,
                                                hostname_, port_,     pathname_,
                                                search_,   hash_};
  return base::ranges::any_of(components,
                              &url_pattern::Component::HasRegExpGroups);
}

// static
int URLPattern::compareComponent(const V8URLPatternComponent& component,
                                 const URLPattern* left,
                                 const URLPattern* right) {
  switch (component.AsEnum()) {
    case V8URLPatternComponent::Enum::kProtocol:
      return url_pattern::Component::Compare(*left->protocol_,
                                             *right->protocol_);
    case V8URLPatternComponent::Enum::kUsername:
      return url_pattern::Component::Compare(*left->username_,
                                             *right->username_);
    case V8URLPatternComponent::Enum::kPassword:
      return url_pattern::Component::Compare(*left->password_,
                                             *right->password_);
    case V8URLPatternComponent::Enum::kHostname:
      return url_pattern::Component::Compare(*left->hostname_,
                                             *right->hostname_);
    case V8URLPatternComponent::Enum::kPort:
      return url_pattern::Component::Compare(*left->port_, *right->port_);
    case V8URLPatternComponent::Enum::kPathname:
      return url_pattern::Component::Compare(*left->pathname_,
                                             *right->pathname_);
    case V8URLPatternComponent::Enum::kSearch:
      return url_pattern::Component::Compare(*left->search_, *right->search_);
    case V8URLPatternComponent::Enum::kHash:
      return url_pattern::Component::Compare(*left->hash_, *right->hash_);
  }
  NOTREACHED_IN_MIGRATION();
}

std::optional<SafeUrlPattern> URLPattern::ToSafeUrlPattern(
    ExceptionState& exception_state) const {
  const std::pair<const url_pattern::Component*, const char*>
      components_with_names[] = {
          {protocol_, "protocol"}, {username_, "username"},
          {password_, "password"}, {hostname_, "hostname"},
          {port_, "port"},         {pathname_, "pathname"},
          {search_, "search"},     {hash_, "hash"}};
  String components_with_regexp;
  for (auto [component, name] : components_with_names) {
    if (component->HasRegExpGroups()) {
      components_with_regexp = components_with_regexp +
                               (components_with_regexp.IsNull() ? "" : ", ") +
                               name + " (" +
                               component->GeneratePatternString() + ")";
    }
  }
  if (!components_with_regexp.IsNull()) {
    exception_state.ThrowTypeError(
        "The pattern cannot contain regexp groups, but did in the following "
        "components: " +
        components_with_regexp);
    return std::nullopt;
  }
  CHECK(!hasRegExpGroups());

  SafeUrlPattern safe_url_pattern;
  safe_url_pattern.protocol = protocol_->PartList();
  safe_url_pattern.username = username_->PartList();
  safe_url_pattern.password = password_->PartList();
  safe_url_pattern.hostname = hostname_->PartList();
  safe_url_pattern.port = port_->PartList();
  safe_url_pattern.pathname = pathname_->PartList();
  safe_url_pattern.search = search_->PartList();
  safe_url_pattern.hash = hash_->PartList();
  safe_url_pattern.options.ignore_case = options_.ignore_case;

  return safe_url_pattern;
}

String URLPattern::ToString() const {
  StringBuilder builder;
  builder.Append("(");
  Vector<String> components = {protocol(), username(), password(), hostname(),
                               port(),     pathname(), search(),   hash()};
  for (wtf_size_t i = 0; i < components.size(); i++) {
    builder.Append(components[i] == g_empty_string ? " " : components[i]);
    if (i != components.size() - 1)
      builder.Append(",");
  }
  builder.Append(")");
  return builder.ReleaseString();
}

void URLPattern::Trace(Visitor* visitor) const {
  visitor->Trace(protocol_);
  visitor->Trace(username_);
  visitor->Trace(password_);
  visitor->Trace(hostname_);
  visitor->Trace(port_);
  visitor->Trace(pathname_);
  visitor->Trace(search_);
  visitor->Trace(hash_);
  ScriptWrappable::Trace(visitor);
}

bool URLPattern::Match(ScriptState* script_state,
                       const V8URLPatternInput* input,
                       const String& base_url,
                       URLPatternResult* result,
                       ExceptionState& exception_state) const {
  // By default each URL component value starts with an empty string.  The
  // given input is then layered on top of these defaults.
  String protocol(g_empty_string);
  String username(g_empty_string);
  String password(g_empty_string);
  String hostname(g_empty_string);
  String port(g_empty_string);
  String pathname(g_empty_string);
  String search(g_empty_string);
  String hash(g_empty_string);

  HeapVector<Member<V8URLPatternInput>> inputs;

  switch (input->GetContentType()) {
    case V8URLPatternInput::ContentType::kURLPatternInit: {
      if (base_url) {
        exception_state.ThrowTypeError(
            "Invalid second argument baseURL '" + base_url +
            "' provided with a URLPatternInit input. Use the "
            "URLPatternInit.baseURL property instead.");
        return false;
      }

      URLPatternInit* init = input->GetAsURLPatternInit();

      inputs.push_back(MakeGarbageCollected<V8URLPatternInput>(init));

      v8::TryCatch try_catch(script_state->GetIsolate());
      // Layer the URLPatternInit values on top of the default empty strings.
      ApplyInit(init, ValueType::kURL, protocol, username, password, hostname,
                port, pathname, search, hash,
                PassThroughException(script_state->GetIsolate()));
      if (try_catch.HasCaught()) {
        // Treat exceptions simply as a failure to match.
        return false;
      }
      break;
    }
    case V8URLPatternInput::ContentType::kUSVString: {
      KURL parsed_base_url(base_url);
      if (base_url && !parsed_base_url.IsValid()) {
        // Treat as failure to match, but don't throw an exception.
        return false;
      }

      const String& input_string = input->GetAsUSVString();

      inputs.push_back(MakeGarbageCollected<V8URLPatternInput>(input_string));
      if (base_url)
        inputs.push_back(MakeGarbageCollected<V8URLPatternInput>(base_url));

      // The compile the input string as a fully resolved URL.
      KURL url(parsed_base_url, input_string);
      if (!url.IsValid() || url.IsEmpty()) {
        // Treat as failure to match, but don't throw an exception.
        return false;
      }

      // Apply the parsed URL components on top of our defaults.
      if (url.Protocol())
        protocol = url.Protocol();
      if (!url.User().empty()) {
        username = url.User().ToString();
      }
      if (!url.Pass().empty()) {
        password = url.Pass().ToString();
      }
      if (!url.Host().empty()) {
        hostname = url.Host().ToString();
      }
      if (url.Port() > 0) {
        port = String::Number(url.Port());
      }
      if (!url.GetPath().empty()) {
        pathname = url.GetPath().ToString();
      }
      if (!url.Query().empty()) {
        search = url.Query().ToString();
      }
      if (url.HasFragmentIdentifier()) {
        hash = url.FragmentIdentifier().ToString();
      }
      break;
    }
  }

  // Declare vectors to hold matched group name/value pairs produced by the
  // matching algorithm.
  Vector<std::pair<String, String>> protocol_group_list;
  Vector<std::pair<String, String>> username_group_list;
  Vector<std::pair<String, String>> password_group_list;
  Vector<std::pair<String, String>> hostname_group_list;
  Vector<std::pair<String, String>> port_group_list;
  Vector<std::pair<String, String>> pathname_group_list;
  Vector<std::pair<String, String>> search_group_list;
  Vector<std::pair<String, String>> hash_group_list;

  // If we are not generating a full result then we don't need to populate
  // group lists.
  auto* protocol_group_list_ref = result ? &protocol_group_list : nullptr;
  auto* username_group_list_ref = result ? &username_group_list : nullptr;
  auto* password_group_list_ref = result ? &password_group_list : nullptr;
  auto* hostname_group_list_ref = result ? &hostname_group_list : nullptr;
  auto* port_group_list_ref = result ? &port_group_list : nullptr;
  auto* pathname_group_list_ref = result ? &pathname_group_list : nullptr;
  auto* search_group_list_ref = result ? &search_group_list : nullptr;
  auto* hash_group_list_ref = result ? &hash_group_list : nullptr;

  CHECK(protocol_);
  CHECK(username_);
  CHECK(password_);
  CHECK(hostname_);
  CHECK(port_);
  CHECK(pathname_);
  CHECK(search_);
  CHECK(hash_);

  // Each component of the pattern must match the corresponding component of
  // the input.
  bool matched = protocol_->Match(protocol, protocol_group_list_ref) &&
                 username_->Match(username, username_group_list_ref) &&
                 password_->Match(password, password_group_list_ref) &&
                 hostname_->Match(hostname, hostname_group_list_ref) &&
                 port_->Match(port, port_group_list_ref) &&
                 pathname_->Match(pathname, pathname_group_list_ref) &&
                 search_->Match(search, search_group_list_ref) &&
                 hash_->Match(hash, hash_group_list_ref);

  if (!matched || !result)
    return matched;

  result->setInputs(std::move(inputs));

  result->setProtocol(MakeURLPatternComponentResult(script_state, protocol,
                                                    protocol_group_list));
  result->setUsername(MakeURLPatternComponentResult(script_state, username,
                                                    username_group_list));
  result->setPassword(MakeURLPatternComponentResult(script_state, password,
                                                    password_group_list));
  result->setHostname(MakeURLPatternComponentResult(script_state, hostname,
                                                    hostname_group_list));
  result->setPort(
      MakeURLPatternComponentResult(script_state, port, port_group_list));
  result->setPathname(MakeURLPatternComponentResult(script_state, pathname,
                                                    pathname_group_list));
  result->setSearch(
      MakeURLPatternComponentResult(script_state, search, search_group_list));
  result->setHash(
      MakeURLPatternComponentResult(script_state, hash, hash_group_list));

  return true;
}

// static

}  // namespace blink
