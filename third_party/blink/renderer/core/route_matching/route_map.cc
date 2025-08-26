// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/route_matching/route_map.h"

#include "base/check_is_test.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/platform/json/json_parser.h"
#include "third_party/blink/renderer/platform/json/json_values.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

// BEGIN Supplement support:

const char RouteMap::kSupplementName[] = "RouteMap";

const RouteMap* RouteMap::Get(const Document& document) {
  return Supplement<Document>::From<RouteMap>(document);
}

RouteMap* RouteMap::Get(Document& document) {
  return Supplement<Document>::From<RouteMap>(document);
}

RouteMap& RouteMap::Ensure(Document& document) {
  RouteMap* route_map = Get(document);
  if (!route_map) {
    route_map = MakeGarbageCollected<RouteMap>(document);
    Supplement<Document>::ProvideTo<RouteMap>(document, route_map);
  }
  return *route_map;
}

// END Supplement support

RouteMap::RouteMap(Document& document) : Supplement<Document>(document) {}
RouteMap::RouteMap() : Supplement<Document>(nullptr) {
  CHECK_IS_TEST();
}

RouteMap::ParseResult RouteMap::ParseAndApplyRoutes(
    const String& route_map_text) {
  RouteMap::ParseResult result = ParseRoutes(route_map_text);
  Document* document = GetSupplementable();
  CHECK(document);
  document->GetStyleEngine().SetNeedsActiveStyleUpdate(*document);
  return result;
}

RouteMap::ParseResult RouteMap::ParseRoutes(const String& route_map_text) {
  constexpr char kPattern[] = "pattern";
  std::unique_ptr<JSONValue> value = ParseJSON(route_map_text);
  // TODO(crbug.com/436805487): Error reporting needs to be specced. Should we
  // make any modifications to the route map at all if there are errors?
  if (!value) {
    return ParseResult(ParseResult::kSyntaxError, "Invalid JSON");
  }

  std::unique_ptr<JSONObject> value_map = JSONObject::From(std::move(value));
  if (!value_map) {
    return ParseResult(ParseResult::kTypeError, "Not a map");
  }

  if (value_map->Get("routes")) {
    JSONArray* routes = value_map->GetArray("routes");
    if (!routes) {
      return ParseResult(ParseResult::kTypeError,
                         "Invalid data type - expected array for routes");
    }

    for (const JSONValue& route_candidate : *routes) {
      const JSONObject* input_route = JSONObject::Cast(&route_candidate);
      if (!input_route) {
        return ParseResult(ParseResult::kTypeError,
                           "Invalid data type - expected map for route entry");
      }

      String name;
      if (!input_route->GetString("name", &name)) {
        return ParseResult(ParseResult::kTypeError,
                           "Invalid data type or missing name entry for route");
      }

      auto it = routes_.find(name);
      Route* route;
      if (it == routes_.end()) {
        // Create a new route entry, but don't add it until we've checked if the
        // data is valid.
        route = MakeGarbageCollected<Route>();
      } else {
        route = it->value;
      }

      if (const JSONArray* patterns = input_route->GetArray(kPattern)) {
        // Parse an array of patterns.
        for (const JSONValue& pattern_candidate : *patterns) {
          String pattern;
          if (!pattern_candidate.AsString(&pattern)) {
            return ParseResult(ParseResult::kTypeError,
                               "Invalid data type for pattern in route entry");
          }
          route->patterns_.push_back(pattern);
        }
      } else {
        // No pattern array. Single pattern entry, then?
        String single_pattern;
        if (!input_route->GetString(kPattern, &single_pattern)) {
          return ParseResult(
              ParseResult::kTypeError,
              "Missing or invalid data type for pattern in route entry");
        }
        route->patterns_.push_back(single_pattern);
      }

      if (it == routes_.end()) {
        routes_.insert(name, route);
      }
    }
  }

  return ParseResult(ParseResult::kSuccess);
}

bool RouteMap::MatchesRoute(const KURL& url, const String& route) const {
  String path = url.GetPath().ToString();
  const auto it = routes_.find(route);
  if (it == routes_.end()) {
    return false;
  }
  for (const String& pattern : it->value->patterns_) {
    if (path.Contains(pattern)) {
      return true;
    }
  }

  return false;
}

HashSet<String> RouteMap::GetActiveRoutes(const KURL& url) const {
  HashSet<String> active_routes;
  String path = url.GetPath().ToString();
  for (const auto& entry : routes_) {
    for (const String& pattern : entry.value->patterns_) {
      // TODO(crbug.com/436805487): This should use URLPattern
      if (path.Contains(pattern)) {
        active_routes.insert(entry.key);
      }
    }
  }
  return active_routes;
}

void RouteMap::Trace(Visitor* v) const {
  v->Trace(routes_);
  Supplement<Document>::Trace(v);
}

}  // namespace blink
