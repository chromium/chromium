// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/document_metadata/document_metadata_extractor.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "components/schema_org/common/metadata.mojom-blink.h"
#include "third_party/blink/public/mojom/document_metadata/document_metadata.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/json/json_parser.h"
#include "third_party/blink/renderer/platform/json/json_values.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

using mojom::blink::WebPage;
using mojom::blink::WebPagePtr;
using schema_org::mojom::blink::Entity;
using schema_org::mojom::blink::EntityPtr;
using schema_org::mojom::blink::Property;
using schema_org::mojom::blink::PropertyPtr;
using schema_org::mojom::blink::Values;
using schema_org::mojom::blink::ValuesPtr;

// App Indexing enforces a max nesting depth of 5. Our top level message
// corresponds to the WebPage, so this only leaves 4 more levels. We will parse
// entites up to this depth, and ignore any further nesting. If an object at the
// max nesting depth has a property corresponding to an entity, that property
// will be dropped. Note that we will still parse json-ld blocks deeper than
// this, but it won't be passed to App Indexing.
constexpr int kMaxDepth = 4;
// Some strings are very long, and we don't currently use those, so limit string
// length to something reasonable to avoid undue pressure on Icing. Note that
// App Indexing supports strings up to length 20k.
constexpr wtf_size_t kMaxStringLength = 200;
// Enforced by App Indexing, so stop processing early if possible.
constexpr wtf_size_t kMaxNumFields = 20;
// Enforced by App Indexing, so stop processing early if possible.
constexpr wtf_size_t kMaxRepeatedSize = 100;

constexpr char kJSONLDKeyType[] = "@type";
constexpr char kJSONLDKeyGraph[] = "@graph";
bool IsSupportedType(AtomicString type) {
  DEFINE_STATIC_LOCAL(
      HashSet<AtomicString>, elements,
      ({// Common types that include addresses.
        AtomicString("AutoDealer"), AtomicString("Hotel"),
        AtomicString("LocalBusiness"), AtomicString("Organization"),
        AtomicString("Person"), AtomicString("Place"),
        AtomicString("PostalAddress"), AtomicString("Product"),
        AtomicString("Residence"), AtomicString("Restaurant"),
        AtomicString("SingleFamilyResidence"),
        // Common types including phone numbers
        AtomicString("Store"), AtomicString("ContactPoint"),
        AtomicString("LodgingBusiness")}));
  return type && elements.Contains(type);
}

void ExtractEntity(const JSONObject&, int recursion_level, Entity&);

bool ParseRepeatedValue(const JSONArray& arr,
                        int recursion_level,
                        ValuesPtr& values) {
  if (arr.size() < 1) {
    return false;
  }

  const JSONValue::ValueType type = arr.at(0)->GetType();
  switch (type) {
    case JSONValue::ValueType::kTypeNull:
      return false;
    case JSONValue::ValueType::kTypeBoolean:
      values = Values::NewBoolValues({});
      break;
    case JSONValue::ValueType::kTypeInteger:
      values = Values::NewLongValues({});
      break;
    // App Indexing doesn't support double type, so just encode its decimal
    // value as a string instead.
    case JSONValue::ValueType::kTypeDouble:
    case JSONValue::ValueType::kTypeString:
      values = Values::NewStringValues({});
      break;
    case JSONValue::ValueType::kTypeObject:
      if (recursion_level + 1 >= kMaxDepth) {
        return false;
      }
      values = Values::NewEntityValues({});
      break;
    case JSONArray::ValueType::kTypeArray:
      // App Indexing doesn't support nested arrays.
      return false;
  }

  const wtf_size_t arr_size = std::min(arr.size(), kMaxRepeatedSize);
  for (wtf_size_t i = 0; i < arr_size; ++i) {
    const JSONValue* const element = arr.at(i);
    if (element->GetType() != type) {
      // App Indexing doesn't support mixed types. If there are mixed
      // types in the parsed object, we will drop the property.
      return false;
    }
    switch (type) {
      case JSONValue::ValueType::kTypeBoolean: {
        bool v;
        element->AsBoolean(&v);
        values->get_bool_values().push_back(v);
        continue;
      }
      case JSONValue::ValueType::kTypeInteger: {
        int v;
        element->AsInteger(&v);
        values->get_long_values().push_back(v);
        continue;
      }
      case JSONValue::ValueType::kTypeDouble: {
        // App Indexing doesn't support double type, so just encode its decimal
        // value as a string instead.
        double v;
        element->AsDouble(&v);
        String s = String::Number(v);
        s.Truncate(kMaxStringLength);
        values->get_string_values().push_back(s);
        continue;
      }
      case JSONValue::ValueType::kTypeString: {
        String v;
        element->AsString(&v);
        v.Truncate(kMaxStringLength);
        values->get_string_values().push_back(v);
        continue;
      }
      case JSONValue::ValueType::kTypeObject: {
        auto entity = Entity::New();
        ExtractEntity(*(JSONObject::Cast(element)), recursion_level + 1,
                      *entity);
        values->get_entity_values().push_back(std::move(entity));
        continue;
      }
      case JSONValue::ValueType::kTypeNull:
      case JSONValue::ValueType::kTypeArray:
        CHECK(false);
    }
  }
  return true;
}

void ExtractEntity(const JSONObject& val, int recursion_level, Entity& entity) {
  if (recursion_level >= kMaxDepth) {
    return;
  }

  String type;
  val.GetString(kJSONLDKeyType, &type);
  if (!type) {
    type = "Thing";
  }
  entity.type = type;
  for (wtf_size_t i = 0; i < std::min(val.size(), kMaxNumFields); ++i) {
    PropertyPtr property = Property::New();
    const JSONObject::Entry& entry = val.at(i);
    property->name = entry.first;
    if (property->name == kJSONLDKeyType) {
      continue;
    }

    bool add_property = true;

    switch (entry.second->GetType()) {
      case JSONValue::ValueType::kTypeBoolean: {
        bool v;
        val.GetBoolean(entry.first, &v);
        property->values = Values::NewBoolValues({v});
      } break;
      case JSONValue::ValueType::kTypeInteger: {
        int v;
        val.GetInteger(entry.first, &v);
        property->values = Values::NewLongValues({v});
      } break;
      case JSONValue::ValueType::kTypeDouble: {
        double v;
        val.GetDouble(entry.first, &v);
        String s = String::Number(v);
        s.Truncate(kMaxStringLength);
        property->values = Values::NewStringValues({s});
      } break;
      case JSONValue::ValueType::kTypeString: {
        String v;
        val.GetString(entry.first, &v);
        v.Truncate(kMaxStringLength);
        property->values = Values::NewStringValues({v});
      } break;
      case JSONValue::ValueType::kTypeObject: {
        if (recursion_level + 1 >= kMaxDepth) {
          add_property = false;
          break;
        }
        Vector<EntityPtr> entities;
        entities.push_back(Entity::New());
        ExtractEntity(*(val.GetJSONObject(entry.first)), recursion_level + 1,
                      *entities[0]);
        property->values = Values::NewEntityValues(std::move(entities));
      } break;
      case JSONValue::ValueType::kTypeArray:
        add_property = ParseRepeatedValue(*(val.GetArray(entry.first)),
                                          recursion_level, property->values);
        break;
      case JSONValue::ValueType::kTypeNull:
        add_property = false;
        break;
    }
    if (add_property)
      entity.properties.push_back(std::move(property));
  }
}

void ExtractTopLevelEntity(const JSONObject& val, Vector<EntityPtr>& entities) {
  // Now we have a JSONObject which corresponds to a single (possibly nested)
  // entity.
  EntityPtr entity = Entity::New();
  String type;
  val.GetString(kJSONLDKeyType, &type);
  if (!IsSupportedType(AtomicString(type))) {
    return;
  }
  ExtractEntity(val, 0, *entity);
  entities.push_back(std::move(entity));
}

void ExtractEntitiesFromArray(const JSONArray& arr,
                              Vector<EntityPtr>& entities) {
  for (wtf_size_t i = 0; i < arr.size(); ++i) {
    const JSONValue* val = arr.at(i);
    if (val->GetType() == JSONValue::ValueType::kTypeObject) {
      ExtractTopLevelEntity(*(JSONObject::Cast(val)), entities);
    }
  }
}

void ExtractEntityFromTopLevelObject(const JSONObject& val,
                                     Vector<EntityPtr>& entities) {
  const JSONArray* graph = val.GetArray(kJSONLDKeyGraph);
  if (graph) {
    ExtractEntitiesFromArray(*graph, entities);
  }
  ExtractTopLevelEntity(val, entities);
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum ExtractionStatus {
  kOK,
  kEmpty,
  kParseFailure,
  kWrongType,
  kMaxValue = kWrongType,
};

ExtractionStatus ExtractMetadata(const Element& root,
                                 Vector<EntityPtr>& entities) {
  for (Element& element : ElementTraversal::DescendantsOf(root)) {
    if (element.HasTagName(html_names::kScriptTag) &&
        element.FastGetAttribute(html_names::kTypeAttr) ==
            "application/ld+json") {
      // TODO(crbug.com/1264024): Deprecate JSON comments here, if possible.
      std::unique_ptr<JSONValue> json =
          ParseJSONWithCommentsDeprecated(element.textContent());
      if (!json) {
        LOG(ERROR) << "Failed to parse json.";
        return ExtractionStatus::kParseFailure;
      }
      switch (json->GetType()) {
        case JSONValue::ValueType::kTypeArray:
          ExtractEntitiesFromArray(*(JSONArray::Cast(json.get())), entities);
          break;
        case JSONValue::ValueType::kTypeObject:
          ExtractEntityFromTopLevelObject(*(JSONObject::Cast(json.get())),
                                          entities);
          break;
        default:
          return ExtractionStatus::kWrongType;
      }
    }
  }
  if (entities.empty()) {
    return ExtractionStatus::kEmpty;
  }
  return ExtractionStatus::kOK;
}

}  // namespace

WebPagePtr DocumentMetadataExtractor::Extract(const Document& document) {
  TRACE_EVENT0("blink", "DocumentMetadataExtractor::Extract");

  if (!document.GetFrame() || !document.GetFrame()->IsMainFrame())
    return nullptr;

  Element* html = document.documentElement();
  if (!html)
    return nullptr;

  WebPagePtr page = WebPage::New();

  // Traverse the DOM tree and extract the metadata.
  ExtractionStatus status = ExtractMetadata(*html, page->entities);
  if (status != ExtractionStatus::kOK) {
    return nullptr;
  }

  page->url = document.Url();
  page->title = document.title();
  return page;
}

}  // namespace blink
