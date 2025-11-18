// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/route_matching/route_map.h"

#include "base/check_is_test.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/route_matching/route.h"
#include "third_party/blink/renderer/core/url_pattern/url_pattern.h"
#include "third_party/blink/renderer/core/url_pattern/url_pattern_utils.h"
#include "third_party/blink/renderer/platform/json/json_parser.h"
#include "third_party/blink/renderer/platform/json/json_values.h"

namespace blink {

namespace {

RouteMap::ParseResult AddPatternToRoute(const Document& document,
                                        Route& route,
                                        const JSONValue& value) {
  base::expected<URLPattern*, String> pattern =
      ParseURLPatternFromJSON(document.GetExecutionContext()->GetIsolate(),
                              value, document.Url(), IGNORE_EXCEPTION);
  if (pattern.has_value()) {
    DCHECK(*pattern);
    route.AddPattern(*pattern);
    return RouteMap::ParseResult(RouteMap::ParseResult::kSuccess);
  }
  return RouteMap::ParseResult(RouteMap::ParseResult::kSyntaxError,
                               pattern.error());
}

}  // anonymous namespace

RouteMap::RouteMap(Document& document) : document_(document) {}
RouteMap::RouteMap() {
  CHECK_IS_TEST();
}

void RouteMap::Trace(Visitor* v) const {
  v->Trace(document_);
  v->Trace(routes_);
  v->Trace(anonymous_routes_);
  ScriptWrappable::Trace(v);
}

Route* RouteMap::get(const String& route_name) {
  const auto it = routes_.find(route_name);
  if (it == routes_.end()) {
    return nullptr;
  }
  return it->value;
}

const RouteMap* RouteMap::Get(const Document* document) {
  if (!document) {
    return nullptr;
  }
  return document->GetRouteMap();
}

RouteMap* RouteMap::Get(Document* document) {
  if (!document) {
    return nullptr;
  }
  return document->GetRouteMap();
}

RouteMap& RouteMap::Ensure(Document& document) {
  RouteMap* route_map = Get(&document);
  if (!route_map) {
    route_map = MakeGarbageCollected<RouteMap>(document);
    document.SetRouteMap(route_map);
  }
  return *route_map;
}

RouteMap::ParseResult RouteMap::ParseAndApplyRoutes(
    const String& route_map_text) {
  RouteMap::ParseResult result = ParseRoutes(route_map_text);
  UpdateActiveRoutes();
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
        route = MakeGarbageCollected<Route>(GetDocument());
      } else {
        route = it->value;
      }

      if (const JSONArray* patterns = input_route->GetArray(kPattern)) {
        // Parse an array of patterns.
        if (patterns->size() == 0) {
          return ParseResult(ParseResult::kTypeError,
                             "Missing pattern in route entry");
        }
        for (const JSONValue& pattern : *patterns) {
          ParseResult result =
              AddPatternToRoute(GetDocument(), *route, pattern);
          if (!result.IsSuccess()) {
            return result;
          }
        }
      } else {
        // No pattern array. Single pattern entry, then?
        const JSONValue* pattern = input_route->Get(kPattern);
        if (!pattern) {
          return ParseResult(ParseResult::kTypeError,
                             "Missing pattern in route entry");
        }
        ParseResult result = AddPatternToRoute(GetDocument(), *route, *pattern);
        if (!result.IsSuccess()) {
          return result;
        }
      }

      if (it == routes_.end()) {
        routes_.insert(name, route);
      }
    }
  }

  return ParseResult(ParseResult::kSuccess);
}

void RouteMap::AddAnonymousRoute(URLPattern* pattern) {
  String pattern_string = pattern->ToString();
  Member<Route>& route =
      anonymous_routes_.insert(pattern_string, nullptr).stored_value->value;
  if (route) {
    return;
  }
  route = MakeGarbageCollected<Route>(GetDocument());
  route->AddPattern(pattern);
  route->UpdateMatchStatus(previous_url_, next_url_);
}

const Route* RouteMap::FindRoute(const String& route_name) const {
  const auto it = routes_.find(route_name);
  return it == routes_.end() ? nullptr : it->value;
}

const Route* RouteMap::FindRoute(const URLPattern* pattern) const {
  String pattern_string = pattern->ToString();
  auto it = anonymous_routes_.find(pattern_string);
  return it == anonymous_routes_.end() ? nullptr : it->value;
}

void RouteMap::UpdateActiveRoutes() {
  bool changed = false;
  for (const auto& entry : routes_) {
    Route& route = *entry.value;
    changed = route.UpdateMatchStatus(previous_url_, next_url_) || changed;
  }
  for (const auto& entry : anonymous_routes_) {
    Route& route = *entry.value;
    changed = route.UpdateMatchStatus(previous_url_, next_url_) || changed;
  }
  if (changed) {
    GetDocument().GetStyleEngine().RoutesMayHaveChanged();
  }
}

void RouteMap::GetActiveRoutes(
    RoutePreposition preposition,
    RouteMatchState::MatchCollection* collection) const {
  collection->clear();
  for (const auto& entry : routes_) {
    Route& route = *entry.value;
    if (route.Matches(preposition)) {
      collection->insert(&route);
    }
  }
  for (const auto& entry : anonymous_routes_) {
    Route& route = *entry.value;
    if (route.Matches(preposition)) {
      collection->insert(&route);
    }
  }
}

}  // namespace blink
