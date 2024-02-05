// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_SHARED_DICTIONARY_SIMPLE_URL_PATTERN_MATCHER_H_
#define SERVICES_NETWORK_SHARED_DICTIONARY_SIMPLE_URL_PATTERN_MATCHER_H_

#include <memory>
#include <string>
#include <string_view>

#include "base/component_export.h"
#include "base/gtest_prod_util.h"
#include "base/types/expected.h"
#include "base/types/pass_key.h"
#include "third_party/liburlpattern/parse.h"
#include "third_party/liburlpattern/pattern.h"

class GURL;

namespace re2 {
class RE2;
}

namespace network {

// This class partially implements URL Pattern spec which is needed for
// Dictionary URL matching of Compression Dictionary Transport.
// https://urlpattern.spec.whatwg.org/
// https://www.ietf.org/archive/id/draft-ietf-httpbis-compression-dictionary-01.html#name-dictionary-url-matching
//
// This class can be created from a constructor string and a base URL.
// And can run the `test` method of URLPattern only with a URL.
//
// This class doesn't support regexp groups, because this class is used in the
// network service, and there is a security concern in allowing user-defined
// regular expressions. Also this class doesn't support setting `ignoreCase`
// option to true.
//
// Note: We share the limitation of regexp groups with ServiceWorker static
// routing API. For that API, we accept URLPattern JS objects without regexp
// groups, and contvet them to blink::SafeUrlPattern structures in Blink, and
// pass them to the browser process via IPC, and evaluate the pattern with
// resource URLs. For Compression Dictionary Transport feature, we parses the
// constructor string and evaluate the pattern with resource URLs in the network
// service. To simplify this class, this class doesn't share any logic with
// ServiceWorker static routing API. But if we will have more usecases of
// URLPattern, we may need to consider combining those logics.
// Context: https://crrev.com/c/5209732/comment/2d53e11c_e4b8c868/
class COMPONENT_EXPORT(NETWORK_SERVICE) SimpleUrlPatternMatcher {
 public:
  // This class partially represents `URLPatternInit` in the URL Pattern spec.
  // https://urlpattern.spec.whatwg.org/#dictdef-urlpatterninit
  // The only difference is that this class doesn't have a `baseURL` field.
  class COMPONENT_EXPORT(NETWORK_SERVICE) PatternInit {
   public:
    PatternInit(std::optional<std::string> protocol,
                std::optional<std::string> username,
                std::optional<std::string> password,
                std::optional<std::string> hostname,
                std::optional<std::string> port,
                std::optional<std::string> pathname,
                std::optional<std::string> search,
                std::optional<std::string> hash);
    ~PatternInit();
    PatternInit(const PatternInit&) = delete;
    PatternInit& operator=(const PatternInit&) = delete;
    PatternInit(PatternInit&&);
    PatternInit& operator=(PatternInit&&);

    bool operator==(const PatternInit& other) const = default;
    bool operator!=(const PatternInit& other) const = default;

    const std::optional<std::string>& protocol() const { return protocol_; }
    const std::optional<std::string>& username() const { return username_; }
    const std::optional<std::string>& password() const { return password_; }
    const std::optional<std::string>& hostname() const { return hostname_; }
    const std::optional<std::string>& port() const { return port_; }
    const std::optional<std::string>& pathname() const { return pathname_; }
    const std::optional<std::string>& search() const { return search_; }
    const std::optional<std::string>& hash() const { return hash_; }

   private:
    std::optional<std::string> protocol_;
    std::optional<std::string> username_;
    std::optional<std::string> password_;
    std::optional<std::string> hostname_;
    std::optional<std::string> port_;
    std::optional<std::string> pathname_;
    std::optional<std::string> search_;
    std::optional<std::string> hash_;
  };

  // This class represents `component` struct in the URL Pattern spec.
  // https://urlpattern.spec.whatwg.org/#component
  class COMPONENT_EXPORT(NETWORK_SERVICE) Component {
   public:
    // Compile a component from a pattern.
    // https://urlpattern.spec.whatwg.org/#compile-a-component
    static base::expected<Component, std::string> Create(
        std::optional<std::string_view> pattern,
        liburlpattern::EncodeCallback encode_callback,
        const liburlpattern::Options& options);

    Component(liburlpattern::Pattern pattern,
              std::unique_ptr<re2::RE2> regex,
              base::PassKey<Component>);
    ~Component();
    Component(const Component&) = delete;
    Component& operator=(const Component&) = delete;
    Component(Component&&);
    Component& operator=(Component&&);

    // This method partially implements `match` method of URLPattern in the
    // URL Pattern spec.
    // https://urlpattern.spec.whatwg.org/#urlpattern-match
    bool Match(std::string_view value) const;

   private:
    liburlpattern::Pattern pattern_;
    // This is null if `pattern_` supports direct matching.
    std::unique_ptr<re2::RE2> regex_;
  };

  // Creates a SimpleUrlPatternMatcher from a constructor string and a base URL.
  // If the constructor string is invalid or it contains regexp group, this
  // method returns an error string.
  static base::expected<std::unique_ptr<SimpleUrlPatternMatcher>, std::string>
  Create(std::string_view constructor_string, const GURL& base_url);

  SimpleUrlPatternMatcher(Component protocol,
                          Component username,
                          Component password,
                          Component hostname,
                          Component port,
                          Component pathname,
                          Component search,
                          Component hash,
                          base::PassKey<SimpleUrlPatternMatcher>);

  SimpleUrlPatternMatcher(const SimpleUrlPatternMatcher&) = delete;
  SimpleUrlPatternMatcher& operator=(const SimpleUrlPatternMatcher&) = delete;
  ~SimpleUrlPatternMatcher();

  // Performs `test` method of URLPattern with a URL.
  // https://urlpattern.spec.whatwg.org/#dom-urlpattern-test
  bool Match(const GURL& url) const;

 private:
  friend class SimpleUrlPatternMatcherTest;

  // Creates a `PatternInit` with the result of parsing a constructor string and
  // apply `base_url`. And returns the result of processing a URLPatternInit,
  // and processing the default port of the protocol component.
  // https://urlpattern.spec.whatwg.org/#parse-a-constructor-string
  // https://urlpattern.spec.whatwg.org/#process-a-urlpatterninit
  //
  // `protocol_component_out` and `protocol_matches_a_special_scheme_flag_out`
  // are optional out parameter. When they are set, this method fills them with
  // a Component for the protocol component and `protocol matches a special
  // scheme flag` in the spec, which are computed while parsing the constructor
  // string.
  // https://urlpattern.spec.whatwg.org/#compute-protocol-matches-a-special-scheme-flag
  static base::expected<PatternInit, std::string> CreatePatternInit(
      std::string_view url_pattern,
      const GURL& base_url,
      std::optional<Component>* protocol_component_out,
      bool* protocol_matches_a_special_scheme_flag_out);

  // Creates a SimpleUrlPatternMatcher from a `PatternInit` and a precomputed
  // protocol component, and a `protocol_matches_a_special_scheme_flag` flag.
  static base::expected<std::unique_ptr<SimpleUrlPatternMatcher>, std::string>
  CreateFromPatternInit(const PatternInit& pattern,
                        std::optional<Component> precomputed_protocol_component,
                        bool protocol_matches_a_special_scheme_flag);

  Component protocol_;
  Component username_;
  Component password_;
  Component hostname_;
  Component port_;
  Component pathname_;
  Component search_;
  Component hash_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_SHARED_DICTIONARY_SIMPLE_URL_PATTERN_MATCHER_H_
