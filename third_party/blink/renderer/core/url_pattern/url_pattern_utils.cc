// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/url_pattern/url_pattern_utils.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_union_urlpatterninit_usvstring.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_url_pattern_init.h"
#include "third_party/blink/renderer/core/url_pattern/url_pattern.h"
#include "third_party/blink/renderer/platform/json/json_parser.h"
#include "third_party/blink/renderer/platform/json/json_values.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/strcat.h"

namespace blink {

base::expected<URLPattern*, String> ParseURLPatternFromJSON(
    v8::Isolate* isolate,
    const JSONValue& pattern_value,
    const KURL& base_url,
    ExceptionState& exception_state) {
  // If pattern_value is a string, then:
  if (String raw_string; pattern_value.AsString(&raw_string)) {
    V8URLPatternInput* url_pattern_input =
        MakeGarbageCollected<V8URLPatternInput>(raw_string);
    return URLPattern::Create(isolate, url_pattern_input, base_url,
                              exception_state);
  }
  // Otherwise, if pattern_value is a map
  if (const JSONObject* pattern_object = JSONObject::Cast(&pattern_value)) {
    // Let init be «[ "baseURL" → serializedBaseURL ]», representing a
    // dictionary of type URLPatternInit.
    URLPatternInit* init = URLPatternInit::Create();
    init->setBaseURL(base_url);

    // For each key -> value of rawPattern:
    for (wtf_size_t i = 0; i < pattern_object->size(); i++) {
      JSONObject::Entry entry = pattern_object->at(i);
      String key = entry.first;
      String value;
      // If value is not a string
      if (!entry.second->AsString(&value)) {
        return base::unexpected(
            "Values for a URL pattern object must be strings.");
      }

      // Set init[key] to value.
      if (key == "protocol") {
        init->setProtocol(value);
      } else if (key == "username") {
        init->setUsername(value);
      } else if (key == "password") {
        init->setPassword(value);
      } else if (key == "hostname") {
        init->setHostname(value);
      } else if (key == "port") {
        init->setPort(value);
      } else if (key == "pathname") {
        init->setPathname(value);
      } else if (key == "search") {
        init->setSearch(value);
      } else if (key == "hash") {
        init->setHash(value);
      } else if (key == "baseURL") {
        init->setBaseURL(value);
      } else {
        return base::unexpected(StrCat(
            {"Invalid key \"", key, "\" for a URL pattern object found."}));
      }
    }

    // Set pattern to the result of constructing a URLPattern using the
    // URLPattern(input, baseURL) constructor steps given init.
    V8URLPatternInput* url_pattern_input =
        MakeGarbageCollected<V8URLPatternInput>(init);
    return URLPattern::Create(isolate, url_pattern_input, exception_state);
  }
  return base::unexpected(
      "Value for \"href_matches\" should either be a "
      "string, an object, or a list of strings and objects.");
}

}  // namespace blink
