// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/route_matching/route_map.h"

#include "base/auto_reset.h"
#include "base/check_is_test.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/route_matching/route.h"
#include "third_party/blink/renderer/core/route_matching/route_event.h"
#include "third_party/blink/renderer/core/url_pattern/url_pattern.h"
#include "third_party/blink/renderer/core/url_pattern/url_pattern_utils.h"
#include "third_party/blink/renderer/platform/json/json_parser.h"
#include "third_party/blink/renderer/platform/json/json_values.h"

namespace blink {

namespace {


}  // anonymous namespace

RouteMap::RouteMap(Document& document) : Supplement<Document>(document) {}
RouteMap::RouteMap() : Supplement<Document>(nullptr) {
  CHECK_IS_TEST();
}

void RouteMap::Trace(Visitor* v) const {
  v->Trace(routes_);
  v->Trace(anonymous_routes_);
  Supplement<Document>::Trace(v);
  ScriptWrappable::Trace(v);
}

Route* RouteMap::get(const String& route_name) {
  const auto it = routes_.find(route_name);
  if (it == routes_.end()) {
    return nullptr;
  }
  return it->value;
}

// BEGIN Supplement support:

const char RouteMap::kSupplementName[] = "RouteMap";

const RouteMap* RouteMap::Get(const Document* document) {
  if (!document) {
    return nullptr;
  }
  return Supplement<Document>::From<RouteMap>(*document);
}

RouteMap* RouteMap::Get(Document* document) {
  if (!document) {
    return nullptr;
  }
  return Supplement<Document>::From<RouteMap>(*document);
}

RouteMap& RouteMap::Ensure(Document& document) {
  RouteMap* route_map = Get(&document);
  if (!route_map) {
    route_map = MakeGarbageCollected<RouteMap>(document);
    Supplement<Document>::ProvideTo<RouteMap>(document, route_map);
  }
  return *route_map;
}

// END Supplement support

RouteMap::ParseResult RouteMap::ParseAndApplyRoutes(
    const String& route_map_text) {
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

      if (name.StartsWith("--")) {
        // Don't clash with CSS @route rules.
        //
        // TODO(crbug.com/436805487): Add a test for this (if support for
        // <script type="routemap"> (this code) actually won't end up getting
        // removed).
        return ParseResult(ParseResult::kTypeError,
                           "Route names cannot start with '--'");
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
          ParseResult result = AddPatternToRoute(*route, pattern);
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
        ParseResult result = AddPatternToRoute(*route, *pattern);
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

void RouteMap::AddRouteFromRule(const String& dashed_ident,
                                URLPattern* url_pattern) {
  DCHECK(dashed_ident.StartsWith("--"));

  if (routes_.find(dashed_ident) != routes_.end()) {
    // TODO(crbug.com/436805487): Handle route modificiation and removal.
    return;
  }
  Route* route = MakeGarbageCollected<Route>(GetDocument());
  route->AddPattern(url_pattern);
  routes_.insert(dashed_ident, route);
  route->UpdateMatchStatus(previous_url_, next_url_);
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
#if DCHECK_IS_ON()
  DCHECK(!is_updating_active_routes_);
  base::AutoReset<bool> is_updating(&is_updating_active_routes_, true);
#endif

  HeapVector<Member<Route>> routes_needing_event;
  bool changed = false;
  for (const auto& entry : routes_) {
    Route& route = *entry.value;
    changed |= UpdateMatchStatus(route, &routes_needing_event);
  }
  for (const auto& entry : anonymous_routes_) {
    Route& route = *entry.value;
    changed |= UpdateMatchStatus(route, &routes_needing_event);
  }

  for (Route* route : routes_needing_event) {
    bool matches_at = route->Matches(NavigationPreposition::kAt);
    AtomicString type(matches_at ? "activate" : "deactivate");
    auto* event = MakeGarbageCollected<RouteEvent>(type);
    event->SetTarget(route);
    route->DispatchEvent(*event);
  }

  if (changed) {
    GetDocument().GetStyleEngine().NavigationsMayHaveChanged();
  }
}

void RouteMap::GetActiveRoutes(
    NavigationPreposition preposition,
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

RouteMap::ParseResult RouteMap::AddPatternToRoute(Route& route,
                                                  const JSONValue& value) {
  base::expected<URLPattern*, String> pattern =
      ParseURLPatternFromJSON(GetDocument().GetExecutionContext()->GetIsolate(),
                              value, GetDocument().Url(), IGNORE_EXCEPTION);
  if (pattern.has_value()) {
    DCHECK(*pattern);
    route.AddPattern(*pattern);
    // TODO(crbug.com/436805487): If we actually end up keeping support for
    // <script type="routemap">, we're missing events here.
    if (route.UpdateMatchStatus(previous_url_, next_url_)) {
      GetDocument().GetStyleEngine().NavigationsMayHaveChanged();
    }
    return RouteMap::ParseResult(RouteMap::ParseResult::kSuccess);
  }
  return RouteMap::ParseResult(RouteMap::ParseResult::kSyntaxError,
                               pattern.error());
}

bool RouteMap::UpdateMatchStatus(
    Route& route,
    HeapVector<Member<Route>>* routes_needing_event) {
  bool matched_at = route.Matches(NavigationPreposition::kAt);
  if (!route.UpdateMatchStatus(previous_url_, next_url_)) {
    return false;
  }
  if (matched_at != route.Matches(NavigationPreposition::kAt)) {
    routes_needing_event->push_back(&route);
  }
  return true;
}

}  // namespace blink
