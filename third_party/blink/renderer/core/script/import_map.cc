// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script/import_map.h"

#include <memory>
#include <utility>
#include "third_party/blink/public/mojom/devtools/console_message.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/script/layered_api.h"
#include "third_party/blink/renderer/core/script/modulator.h"
#include "third_party/blink/renderer/core/script/parsed_specifier.h"
#include "third_party/blink/renderer/platform/json/json_parser.h"
#include "third_party/blink/renderer/platform/json/json_values.h"
#include "third_party/blink/renderer/platform/loader/fetch/console_logger.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

// We implement two variants of specs:
// - When |support_builtin_modules| is false, import maps without built-in
//   module & fallback supports are implemented.
//   This follows the ToT spec https://wicg.github.io/import-maps/, which is
//   marked by <spec> tags.
// - When |support_builtin_modules| is true, import maps with built-in module &
//   fallback supports are implemented.
//   This basically follows the spec before
//   https://github.com/WICG/import-maps/pull/176, which is marked as
//   [Spec w/ Built-in].
//   This is needed for the fallback mechanism for built-in modules, which was
//   temporarily removed from the spec but is still implemented behind the flag.

namespace blink {

namespace {

// TODO(https://crbug.com/928549): Audit and improve error messages throughout
// this file.

void AddIgnoredKeyMessage(ConsoleLogger& logger,
                          const String& key,
                          const String& reason) {
  logger.AddConsoleMessage(
      mojom::ConsoleMessageSource::kOther, mojom::ConsoleMessageLevel::kWarning,
      "Ignored an import map key \"" + key + "\": " + reason);
}

void AddIgnoredValueMessage(ConsoleLogger& logger,
                            const String& key,
                            const String& reason) {
  logger.AddConsoleMessage(
      mojom::ConsoleMessageSource::kOther, mojom::ConsoleMessageLevel::kWarning,
      "Ignored an import map value of \"" + key + "\": " + reason);
}

// <specdef
// href="https://wicg.github.io/import-maps/#normalize-a-specifier-key">
String NormalizeSpecifierKey(const String& key_string,
                             const KURL& base_url,
                             bool support_builtin_modules,
                             ConsoleLogger& logger) {
  // <spec step="1">If specifierKey is the empty string, then:</spec>
  if (key_string.IsEmpty()) {
    // <spec step="1.1">Report a warning to the console that specifier keys
    // cannot be the empty string.</spec>
    AddIgnoredKeyMessage(logger, key_string,
                         "specifier keys cannot be the empty string.");

    // <spec step="1.2">Return null.</spec>
    return String();
  }

  // <spec step="2">Let url be the result of parsing a URL-like import
  // specifier, given specifierKey and baseURL.</spec>
  ParsedSpecifier key =
      ParsedSpecifier::Create(key_string, base_url, support_builtin_modules);

  switch (key.GetType()) {
    case ParsedSpecifier::Type::kInvalid:
    case ParsedSpecifier::Type::kBare:
      // <spec step="4">Return specifierKey.</spec>
      return key_string;

    case ParsedSpecifier::Type::kURL:
      // <spec step="3">If url is not null, then return the serialization of
      // url.</spec>
      return key.GetImportMapKeyString();
  }
}

// Step 2.4-2.7 of
// <specdef
// href="https://wicg.github.io/import-maps/#sort-and-normalize-a-specifier-map">
KURL NormalizeValue(const String& key,
                    const String& value_string,
                    const KURL& base_url,
                    bool support_builtin_modules,
                    ConsoleLogger& logger) {
  // <spec step="2.4">Let addressURL be the result of parsing a URL-like import
  // specifier given value and baseURL.</spec>
  ParsedSpecifier value =
      ParsedSpecifier::Create(value_string, base_url, support_builtin_modules);

  switch (value.GetType()) {
    case ParsedSpecifier::Type::kInvalid:
      // <spec step="2.5">If addressURL is null, then:</spec>
      //
      // <spec step="2.5.1">Report a warning to the console that the address was
      // invalid.</spec>
      AddIgnoredValueMessage(logger, key, "Invalid URL: " + value_string);
      // <spec step="2.5.2">Continue.</spec>
      return NullURL();

    case ParsedSpecifier::Type::kBare:
      AddIgnoredValueMessage(logger, key, "Bare specifier: " + value_string);
      return NullURL();

    case ParsedSpecifier::Type::kURL:
      // <spec step="2.6">If specifierKey ends with U+002F (/), and the
      // serialization of addressURL does not end with U+002F (/), then:</spec>
      if (key.EndsWith("/") && !value.GetUrl().GetString().EndsWith("/")) {
        // <spec step="2.6.1">Report a warning to the console that an invalid
        // address was given for the specifier key specifierKey; since
        // specifierKey ended in a slash, so must the address.</spec>
        AddIgnoredValueMessage(
            logger, key,
            "Since specifierKey ended in a slash, so must the address: " +
                value_string);

        // <spec step="2.6.2">Continue.</spec>
        return NullURL();
      }

      return value.GetUrl();
  }
}

}  // namespace

// <specdef
// href="https://wicg.github.io/import-maps/#parse-an-import-map-string">
//
// Parse |text| as an import map. Errors (e.g. json parsing error, invalid
// keys/values, etc.) are basically ignored, except that they are reported to
// the console |logger|.
ImportMap* ImportMap::Parse(const Modulator& modulator,
                            const String& input,
                            const KURL& base_url,
                            bool support_builtin_modules,
                            ConsoleLogger& logger,
                            ScriptValue* error_to_rethrow) {
  DCHECK(error_to_rethrow);

  // <spec step="1">Let parsed be the result of parsing JSON into Infra values
  // given input.</spec>
  std::unique_ptr<JSONValue> parsed = ParseJSON(input);

  if (!parsed) {
    *error_to_rethrow =
        modulator.CreateSyntaxError("Failed to parse import map: invalid JSON");
    return MakeGarbageCollected<ImportMap>();
  }

  // <spec step="2">If parsed is not a map, then throw a TypeError indicating
  // that the top-level value must be a JSON object.</spec>
  std::unique_ptr<JSONObject> parsed_map = JSONObject::From(std::move(parsed));
  if (!parsed_map) {
    *error_to_rethrow =
        modulator.CreateTypeError("Failed to parse import map: not an object");
    return MakeGarbageCollected<ImportMap>();
  }

  // <spec step="3">Let sortedAndNormalizedImports be an empty map.</spec>
  SpecifierMap sorted_and_normalized_imports;

  // <spec step="4">If parsed["imports"] exists, then:</spec>
  if (parsed_map->Get("imports")) {
    // <spec step="4.1">If parsed["imports"] is not a map, then throw a
    // TypeError indicating that the "imports" top-level key must be a JSON
    // object.</spec>
    JSONObject* imports = parsed_map->GetJSONObject("imports");
    if (!imports) {
      *error_to_rethrow = modulator.CreateTypeError(
          "Failed to parse import map: \"imports\" "
          "top-level key must be a JSON object.");
      return MakeGarbageCollected<ImportMap>();
    }

    // <spec step="4.2">Set sortedAndNormalizedImports to the result of sorting
    // and normalizing a specifier map given parsed["imports"] and
    // baseURL.</spec>
    sorted_and_normalized_imports = SortAndNormalizeSpecifierMap(
        imports, base_url, support_builtin_modules, logger);
  }

  // <spec step="5">Let sortedAndNormalizedScopes be an empty map.</spec>
  ScopeType sorted_and_normalized_scopes;

  // <spec step="6">If parsed["scopes"] exists, then:</spec>
  if (parsed_map->Get("scopes")) {
    // <spec step="6.1">If parsed["scopes"] is not a map, then throw a TypeError
    // indicating that the "scopes" top-level key must be a JSON object.</spec>
    JSONObject* scopes = parsed_map->GetJSONObject("scopes");
    if (!scopes) {
      *error_to_rethrow = modulator.CreateTypeError(
          "Failed to parse import map: \"scopes\" "
          "top-level key must be a JSON object.");
      return MakeGarbageCollected<ImportMap>();
    }

    // <spec step="6.2">Set sortedAndNormalizedScopes to the result of sorting
    // and normalizing scopes given parsed["scopes"] and baseURL.</spec>

    // <specdef label="sort-and-normalize-scopes"
    // href="https://wicg.github.io/import-maps/#sort-and-normalize-scopes">

    // <spec label="sort-and-normalize-scopes" step="1">Let normalized be an
    // empty map.</spec>
    ScopeType normalized;

    // <spec label="sort-and-normalize-scopes" step="2">For each scopePrefix →
    // potentialSpecifierMap of originalMap,</spec>
    for (wtf_size_t i = 0; i < scopes->size(); ++i) {
      const JSONObject::Entry& entry = scopes->at(i);

      JSONObject* specifier_map = scopes->GetJSONObject(entry.first);
      if (!specifier_map) {
        // <spec label="sort-and-normalize-scopes" step="2.1">If
        // potentialSpecifierMap is not a map, then throw a TypeError indicating
        // that the value of the scope with prefix scopePrefix must be a JSON
        // object.</spec>
        *error_to_rethrow = modulator.CreateTypeError(
            "Failed to parse import map: the value of the scope with prefix "
            "\"" +
            entry.first + "\" must be a JSON object.");
        return MakeGarbageCollected<ImportMap>();
      }

      // <spec label="sort-and-normalize-scopes" step="2.2">Let scopePrefixURL
      // be the result of parsing scopePrefix with baseURL as the base
      // URL.</spec>
      const KURL prefix_url(base_url, entry.first);

      // <spec label="sort-and-normalize-scopes" step="2.3">If scopePrefixURL is
      // failure, then:</spec>
      if (!prefix_url.IsValid()) {
        // <spec label="sort-and-normalize-scopes" step="2.3.1">Report a warning
        // to the console that the scope prefix URL was not parseable.</spec>
        logger.AddConsoleMessage(
            mojom::ConsoleMessageSource::kOther,
            mojom::ConsoleMessageLevel::kWarning,
            "Ignored scope \"" + entry.first + "\": not parsable as a URL.");

        // <spec label="sort-and-normalize-scopes" step="2.3.2">Continue.</spec>
        continue;
      }

      // <spec label="sort-and-normalize-scopes" step="2.5">Let
      // normalizedScopePrefix be the serialization of scopePrefixURL.</spec>
      //
      // <spec label="sort-and-normalize-scopes" step="2.6">Set
      // normalized[normalizedScopePrefix] to the result of sorting and
      // normalizing a specifier map given potentialSpecifierMap and
      // baseURL.</spec>
      sorted_and_normalized_scopes.push_back(std::make_pair(
          prefix_url.GetString(),
          SortAndNormalizeSpecifierMap(specifier_map, base_url,
                                       support_builtin_modules, logger)));
    }
    // <spec label="sort-and-normalize-scopes" step="3">Return the result of
    // sorting normalized, with an entry a being less than an entry b if a’s key
    // is longer or code unit less than b’s key.</spec>
    std::sort(sorted_and_normalized_scopes.begin(),
              sorted_and_normalized_scopes.end(),
              [](const ScopeEntryType& a, const ScopeEntryType& b) {
                return CodeUnitCompareLessThan(b.first, a.first);
              });
  }

  // TODO(hiroshige): Implement Step 7.

  // <spec step="8">Return the import map whose imports are
  // sortedAndNormalizedImports and whose scopes scopes are
  // sortedAndNormalizedScopes.</spec>
  return MakeGarbageCollected<ImportMap>(
      modulator, support_builtin_modules,
      std::move(sorted_and_normalized_imports),
      std::move(sorted_and_normalized_scopes));
}

// <specdef
// href="https://wicg.github.io/import-maps/#sort-and-normalize-a-specifier-map">
ImportMap::SpecifierMap ImportMap::SortAndNormalizeSpecifierMap(
    const JSONObject* imports,
    const KURL& base_url,
    bool support_builtin_modules,
    ConsoleLogger& logger) {
  // <spec step="1">Let normalized be an empty map.</spec>
  SpecifierMap normalized;

  // <spec step="2">For each specifierKey → value of originalMap,</spec>
  for (wtf_size_t i = 0; i < imports->size(); ++i) {
    const JSONObject::Entry& entry = imports->at(i);

    // <spec step="2.1">Let normalizedSpecifierKey be the result of normalizing
    // a specifier key given specifierKey and baseURL.</spec>
    const String normalized_specifier_key = NormalizeSpecifierKey(
        entry.first, base_url, support_builtin_modules, logger);

    // <spec step="2.2">If normalizedSpecifierKey is null, then continue.</spec>
    if (normalized_specifier_key.IsEmpty())
      continue;

    Vector<KURL> values;
    switch (entry.second->GetType()) {
      case JSONValue::ValueType::kTypeNull:
        if (!support_builtin_modules) {
          // <spec step="2.3">If value is not a string, then:</spec>
          //
          // <spec step="2.3.1">Report a warning to the console that addresses
          // must be strings.</spec>
          AddIgnoredValueMessage(logger, entry.first, "Invalid value type.");

          // <spec step="2.3.2">Continue.</spec>
          continue;
        }
        // [Spec w/ Built-in] Otherwise, if value is null, then set
        // normalized[normalizedSpecifierKey] to a new empty list.
        break;

      case JSONValue::ValueType::kTypeBoolean:
      case JSONValue::ValueType::kTypeInteger:
      case JSONValue::ValueType::kTypeDouble:
      case JSONValue::ValueType::kTypeObject:
        // <spec step="2.3">If value is not a string, then:</spec>
        //
        // <spec step="2.3.1">Report a warning to the console that addresses
        // must be strings.</spec>
        AddIgnoredValueMessage(logger, entry.first, "Invalid value type.");

        // <spec step="2.3.2">Continue.</spec>
        //
        // By continuing here, we leave |normalized[normalized_specifier_key]|
        // unset, and continue processing.
        continue;

      case JSONValue::ValueType::kTypeString: {
        // Steps 2.4-2.6 are implemented in NormalizeValue().
        String value_string;
        if (!imports->GetString(entry.first, &value_string)) {
          AddIgnoredValueMessage(logger, entry.first,
                                 "Internal error in GetString().");
          break;
        }
        KURL value = NormalizeValue(entry.first, value_string, base_url,
                                    support_builtin_modules, logger);

        // <spec step="2.7">Set normalized[specifierKey] to addressURL.</spec>
        if (value.IsValid())
          values.push_back(value);
        break;
      }

      case JSONValue::ValueType::kTypeArray: {
        if (!support_builtin_modules) {
          // <spec step="2.3">If value is not a string, then:</spec>
          //
          // <spec step="2.3.1">Report a warning to the console that addresses
          // must be strings.</spec>
          AddIgnoredValueMessage(logger, entry.first, "Invalid value type.");

          // <spec step="2.3.2">Continue.</spec>
          continue;
        }

        // [Spec w/ Built-in] Otherwise, if value is a list, then set
        // normalized[normalizedSpecifierKey] to value.
        JSONArray* array = imports->GetArray(entry.first);
        if (!array) {
          AddIgnoredValueMessage(logger, entry.first,
                                 "Internal error in GetArray().");
          break;
        }

        // <spec step="2">For each specifierKey → value of ...</spec>
        for (wtf_size_t j = 0; j < array->size(); ++j) {
          // <spec step="2.3">If value is not a string, then:</spec>
          String value_string;
          if (!array->at(j)->AsString(&value_string)) {
            // <spec step="2.3.1">Report a warning to the console that addresses
            // must be strings.</spec>
            AddIgnoredValueMessage(logger, entry.first,
                                   "Non-string in the value.");
            // <spec step="2.3.2">Continue.</spec>
            continue;
          }
          KURL value = NormalizeValue(entry.first, value_string, base_url,
                                      support_builtin_modules, logger);

          if (value.IsValid())
            values.push_back(value);
        }
        break;
      }
    }

    if (!support_builtin_modules) {
      DCHECK_LE(values.size(), 1u);
    }

    // TODO(hiroshige): Move these checks to resolution time.
    if (values.size() > 2) {
      AddIgnoredValueMessage(logger, entry.first,
                             "An array of length > 2 is not yet supported.");
      values.clear();
    }
    if (values.size() == 2) {
      if (layered_api::GetBuiltinPath(values[0]).IsNull()) {
        AddIgnoredValueMessage(
            logger, entry.first,
            "Fallback from a non-builtin URL is not yet supported.");
        values.clear();
      } else if (normalized_specifier_key != values[1]) {
        AddIgnoredValueMessage(logger, entry.first,
                               "Fallback URL should match the original URL.");
        values.clear();
      }
    }

    // <spec step="2.7">Set normalized[specifierKey] to addressURL.</spec>
    normalized.Set(normalized_specifier_key, values);
  }

  return normalized;
}

// <specdef href="https://wicg.github.io/import-maps/#resolve-an-imports-match">
base::Optional<ImportMap::MatchResult> ImportMap::MatchPrefix(
    const ParsedSpecifier& parsed_specifier,
    const SpecifierMap& specifier_map) const {
  // Do not perform prefix match for non-bare specifiers.
  if (parsed_specifier.GetType() != ParsedSpecifier::Type::kBare)
    return base::nullopt;

  const String key = parsed_specifier.GetImportMapKeyString();

  // Prefix match, i.e. "Packages" via trailing slashes.
  // https://github.com/WICG/import-maps#packages-via-trailing-slashes
  //
  // TODO(hiroshige): optimize this if necessary. See
  // https://github.com/WICG/import-maps/issues/73#issuecomment-439327758
  // for some candidate implementations.

  // "most-specific wins", i.e. when there are multiple matching keys,
  // choose the longest.
  // https://github.com/WICG/import-maps/issues/102
  base::Optional<MatchResult> best_match;

  // <spec step="1">For each specifierKey → address of specifierMap,</spec>
  for (auto it = specifier_map.begin(); it != specifier_map.end(); ++it) {
    // <spec step="1.2">If specifierKey ends with U+002F (/) and
    // normalizedSpecifier starts with specifierKey, then:</spec>
    if (!it->key.EndsWith('/'))
      continue;

    if (!key.StartsWith(it->key))
      continue;

    // https://wicg.github.io/import-maps/#longer-or-code-unit-less-than
    // We omit code unit comparison, because there can be at most one
    // prefix-matching entry with the same length.
    if (best_match && it->key.length() < (*best_match)->key.length())
      continue;

    best_match = it;
  }
  return best_match;
}

ImportMap::ImportMap()
    : support_builtin_modules_(false),
      modulator_for_built_in_modules_(nullptr) {}

ImportMap::ImportMap(const Modulator& modulator_for_built_in_modules,
                     bool support_builtin_modules,
                     SpecifierMap&& imports,
                     ScopeType&& scopes)
    : imports_(std::move(imports)),
      scopes_(std::move(scopes)),
      support_builtin_modules_(support_builtin_modules),
      modulator_for_built_in_modules_(&modulator_for_built_in_modules) {}

// <specdef
// href="https://wicg.github.io/import-maps/#resolve-a-module-specifier">
base::Optional<KURL> ImportMap::Resolve(const ParsedSpecifier& parsed_specifier,
                                        const KURL& base_url,
                                        String* debug_message) const {
  DCHECK(debug_message);

  // <spec step="8">For each scopePrefix → scopeImports of importMap’s
  // scopes,</spec>
  for (const auto& entry : scopes_) {
    // <spec step="8.1">If scopePrefix is baseURLString, or if scopePrefix ends
    // with U+002F (/) and baseURLString starts with scopePrefix, then:</spec>
    if (entry.first == base_url.GetString() ||
        (entry.first.EndsWith("/") &&
         base_url.GetString().StartsWith(entry.first))) {
      // <spec step="8.1.1">Let scopeImportsMatch be the result of resolving an
      // imports match given normalizedSpecifier and scopeImports.</spec>
      base::Optional<KURL> scope_match =
          ResolveImportsMatch(parsed_specifier, entry.second, debug_message);

      // <spec step="8.1.2">If scopeImportsMatch is not null, then return
      // scopeImportsMatch.</spec>
      if (scope_match)
        return scope_match;
    }
  }

  // <spec step="9">Let topLevelImportsMatch be the result of resolving an
  // imports match given normalizedSpecifier and importMap’s imports.</spec>
  //
  // <spec step="10">If topLevelImportsMatch is not null, then return
  // topLevelImportsMatch.</spec>
  return ResolveImportsMatch(parsed_specifier, imports_, debug_message);
}

// <specdef href="https://wicg.github.io/import-maps/#resolve-an-imports-match">
base::Optional<KURL> ImportMap::ResolveImportsMatch(
    const ParsedSpecifier& parsed_specifier,
    const SpecifierMap& specifier_map,
    String* debug_message) const {
  DCHECK(debug_message);
  const String key = parsed_specifier.GetImportMapKeyString();

  // <spec step="1.1">If specifierKey is normalizedSpecifier, then ...</spec>
  MatchResult exact = specifier_map.find(key);
  if (exact != specifier_map.end()) {
    return ResolveImportsMatchInternal(key, exact, debug_message);
  }

  // Step 1.2.
  if (auto prefix_match = MatchPrefix(parsed_specifier, specifier_map)) {
    return ResolveImportsMatchInternal(key, *prefix_match, debug_message);
  }

  // <spec step="2">Return null.</spec>
  *debug_message = "Import Map: \"" + key +
                   "\" matches with no entries and thus is not mapped.";
  return base::nullopt;
}

// <specdef href="https://wicg.github.io/import-maps/#resolve-an-imports-match">
base::Optional<KURL> ImportMap::ResolveImportsMatchInternal(
    const String& key,
    const MatchResult& matched,
    String* debug_message) const {
  // <spec step="1.2.1">Let afterPrefix be the portion of normalizedSpecifier
  // after the initial specifierKey prefix.</spec>
  const String after_prefix = key.Substring(matched->key.length());

  for (const KURL& value : matched->value) {
    // <spec step="1.1">If specifierKey is normalizedSpecifier, then return
    // address.</spec>
    //
    // <spec step="1.2">If specifierKey ends with U+002F (/) and
    // normalizedSpecifier starts with specifierKey, then:</spec>
    //
    // <spec step="1.2.3">Let url be the result of parsing afterPrefix relative
    // to the base URL address.</spec>
    const KURL url = after_prefix.IsEmpty() ? value : KURL(value, after_prefix);

    // [Spec w/ Built-in] Return url0, if moduleMap[url0] exists; otherwise,
    // return url1.
    //
    // Note: Here we filter out non-existing built-in modules in all cases.
    if (!support_builtin_modules_ ||
        layered_api::ResolveFetchingURL(*modulator_for_built_in_modules_, url)
            .IsValid()) {
      *debug_message = "Import Map: \"" + key + "\" matches with \"" +
                       matched->key + "\" and is mapped to " +
                       url.ElidedString();

      // <spec step="1.2.4">If url is failure, then return null.</spec>
      //
      // <spec step="1.2.5">Return url.</spec>
      return url;
    }
  }

  // [Spec w/ Built-in] If addresses’s size is 0, then throw a TypeError
  // indicating that normalizedSpecifier was mapped to no addresses.
  *debug_message = "Import Map: \"" + key + "\" matches with \"" +
                   matched->key + "\" but fails to be mapped (no viable URLs)";
  return NullURL();
}

static void SpecifierMapToString(StringBuilder& builder,
                                 bool support_builtin_modules,
                                 const ImportMap::SpecifierMap& specifier_map) {
  builder.Append("{");
  bool is_first_key = true;
  for (const auto& it : specifier_map) {
    if (!is_first_key)
      builder.Append(",");
    is_first_key = false;
    builder.Append(it.key.EncodeForDebugging());
    builder.Append(":");
    if (support_builtin_modules) {
      builder.Append("[");
      bool is_first_value = true;
      for (const auto& v : it.value) {
        if (!is_first_value)
          builder.Append(",");
        is_first_value = false;
        builder.Append(v.GetString().EncodeForDebugging());
      }
      builder.Append("]");
    } else {
      if (it.value.size() == 0) {
        builder.Append("[]");
      } else {
        DCHECK_EQ(it.value.size(), 1u);
        builder.Append(it.value[0].GetString().EncodeForDebugging());
      }
    }
  }
  builder.Append("}");
}

String ImportMap::ToString() const {
  StringBuilder builder;
  builder.Append("{\"imports\":");
  SpecifierMapToString(builder, support_builtin_modules_, imports_);

  builder.Append(",\"scopes\":{");

  bool is_first_scope = true;
  for (const auto& entry : scopes_) {
    if (!is_first_scope)
      builder.Append(",");
    is_first_scope = false;
    builder.Append(entry.first.EncodeForDebugging());
    builder.Append(":");
    SpecifierMapToString(builder, support_builtin_modules_, entry.second);
  }

  builder.Append("}");

  builder.Append("}");

  return builder.ToString();
}

void ImportMap::Trace(Visitor* visitor) {
  visitor->Trace(modulator_for_built_in_modules_);
}

}  // namespace blink
