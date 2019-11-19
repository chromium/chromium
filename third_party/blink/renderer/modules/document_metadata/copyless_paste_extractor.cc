// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/document_metadata/copyless_paste_extractor.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "third_party/blink/public/mojom/document_metadata/copyless_paste.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/json/json_parser.h"
#include "third_party/blink/renderer/platform/json/json_values.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

using mojom::document_metadata::blink::Entity;
using mojom::document_metadata::blink::EntityPtr;
using mojom::document_metadata::blink::Property;
using mojom::document_metadata::blink::PropertyPtr;
using mojom::document_metadata::blink::Values;
using mojom::document_metadata::blink::ValuesPtr;
using mojom::document_metadata::blink::WebPage;
using mojom::document_metadata::blink::WebPagePtr;

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
  DEFINE_STATIC_LOCAL(HashSet<AtomicString>, elements,
                      ({// Common types that include addresses.
                        "AutoDealer", "Hotel", "LocalBusiness", "Organization",
                        "Person", "Place", "PostalAddress", "Product",
                        "Residence", "Restaurant", "SingleFamilyResidence",
                        // Common types including phone numbers
                        "Store", "ContactPoint", "LodgingBusiness"}));
  return type && elements.Contains(type);
}

void ExtractEntity(const JSONObject&, Entity&, int recursionLevel);

bool ParseRepeatedValue(const JSONArray& arr,
                        Values& values,
                        int recursionLevel) {
  if (arr.size() < 1) {
    return false;
  }

  const JSONValue::ValueType type = arr.at(0)->GetType();
  switch (type) {
    case JSONValue::ValueType::kTypeBoolean:
      values.set_bool_values(Vector<bool>());
      break;
    case JSONValue::ValueType::kTypeInteger:
      values.set_long_values(Vector<int64_t>());
      break;
    case JSONValue::ValueType::kTypeDouble:
      // App Indexing doesn't support double type, so just encode its decimal
      // value as a string instead.
      values.set_string_values(Vector<String>());
      break;
    case JSONValue::ValueType::kTypeString:
      values.set_string_values(Vector<String>());
      break;
    case JSONValue::ValueType::kTypeObject:
      if (recursionLevel + 1 >= kMaxDepth) {
        return false;
      }
      values.set_entity_values(Vector<EntityPtr>());
      break;
    case JSONArray::ValueType::kTypeArray:
      // App Indexing doesn't support nested arrays.
      return false;
    default:
      break;
  }
  for (wtf_size_t j = 0; j < std::min(arr.size(), kMaxRepeatedSize); ++j) {
    const JSONValue* innerVal = arr.at(j);
    if (innerVal->GetType() != type) {
      // App Indexing doesn't support mixed types. If there are mixed
      // types in the parsed object, we will drop the property.
      return false;
    }
    switch (innerVal->GetType()) {
      case JSONValue::ValueType::kTypeBoolean: {
        bool v;
        innerVal->AsBoolean(&v);
        values.get_bool_values().push_back(v);
      } break;
      case JSONValue::ValueType::kTypeInteger: {
        int v;
        innerVal->AsInteger(&v);
        values.get_long_values().push_back(v);
      } break;
      case JSONValue::ValueType::kTypeDouble: {
        // App Indexing doesn't support double type, so just encode its decimal
        // value as a string instead.
        double v;
        innerVal->AsDouble(&v);
        String s = String::Number(v);
        s.Truncate(kMaxStringLength);
        values.get_string_values().push_back(s);
      } break;
      case JSONValue::ValueType::kTypeString: {
        String v;
        innerVal->AsString(&v);
        v.Truncate(kMaxStringLength);
        values.get_string_values().push_back(v);
      } break;
      case JSONValue::ValueType::kTypeObject:
        values.get_entity_values().push_back(Entity::New());
        ExtractEntity(*(JSONObject::Cast(innerVal)),
                      *(values.get_entity_values().at(j)), recursionLevel + 1);
        break;
      default:
        break;
    }
  }
  return true;
}

void ExtractEntity(const JSONObject& val, Entity& entity, int recursionLevel) {
  if (recursionLevel >= kMaxDepth) {
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
    property->values = Values::New();

    bool addProperty = true;

    switch (entry.second->GetType()) {
      case JSONValue::ValueType::kTypeBoolean: {
        bool v;
        val.GetBoolean(entry.first, &v);
        property->values->set_bool_values({v});
      } break;
      case JSONValue::ValueType::kTypeInteger: {
        int v;
        val.GetInteger(entry.first, &v);
        property->values->set_long_values({v});
      } break;
      case JSONValue::ValueType::kTypeDouble: {
        double v;
        val.GetDouble(entry.first, &v);
        String s = String::Number(v);
        s.Truncate(kMaxStringLength);
        property->values->set_string_values({s});
      } break;
      case JSONValue::ValueType::kTypeString: {
        String v;
        val.GetString(entry.first, &v);
        v.Truncate(kMaxStringLength);
        property->values->set_string_values({v});
      } break;
      case JSONValue::ValueType::kTypeObject: {
        if (recursionLevel + 1 >= kMaxDepth) {
          addProperty = false;
          break;
        }
        property->values->set_entity_values(Vector<EntityPtr>());
        property->values->get_entity_values().push_back(Entity::New());

        ExtractEntity(*(val.GetJSONObject(entry.first)),
                      *(property->values->get_entity_values().at(0)),
                      recursionLevel + 1);
      } break;
      case JSONValue::ValueType::kTypeArray:
        addProperty = ParseRepeatedValue(*(val.GetArray(entry.first)),
                                         *(property->values), recursionLevel);
        break;
      default:
        break;
    }
    if (addProperty)
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
  ExtractEntity(val, *entity, 0);
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

// ExtractionStatus is used in UMA, hence is append-only.
// kCount must be the last entry.
enum ExtractionStatus { kOK, kEmpty, kParseFailure, kWrongType, kCount };

ExtractionStatus ExtractMetadata(const Element& root,
                                 Vector<EntityPtr>& entities) {
  for (Element& element : ElementTraversal::DescendantsOf(root)) {
    if (element.HasTagName(html_names::kScriptTag) &&
        element.FastGetAttribute(html_names::kTypeAttr) ==
            "application/ld+json") {
      std::unique_ptr<JSONValue> json = ParseJSON(element.textContent());
      if (!json) {
        LOG(ERROR) << "Failed to parse json.";
        return kParseFailure;
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
          return kWrongType;
      }
    }
  }
  if (entities.IsEmpty()) {
    return kEmpty;
  }
  return kOK;
}

}  // namespace

WebPagePtr CopylessPasteExtractor::Extract(const Document& document) {
  TRACE_EVENT0("blink", "CopylessPasteExtractor::Extract");

  if (!document.GetFrame() || !document.GetFrame()->IsMainFrame())
    return nullptr;

  Element* html = document.documentElement();
  if (!html)
    return nullptr;

  WebPagePtr page = WebPage::New();

  // Traverse the DOM tree and extract the metadata.
  base::TimeTicks start_time = base::TimeTicks::Now();
  ExtractionStatus status = ExtractMetadata(*html, page->entities);
  base::TimeDelta elapsed_time = base::TimeTicks::Now() - start_time;

  DEFINE_STATIC_LOCAL(EnumerationHistogram, status_histogram,
                      ("CopylessPaste.ExtractionStatus", kCount));
  status_histogram.Count(status);

  if (status != kOK) {
    DEFINE_STATIC_LOCAL(
        CustomCountHistogram, extraction_histogram,
        ("CopylessPaste.ExtractionFailedUs", 1, 1000 * 1000, 50));
    extraction_histogram.CountMicroseconds(elapsed_time);
    return nullptr;
  }
  DEFINE_STATIC_LOCAL(CustomCountHistogram, extraction_histogram,
                      ("CopylessPaste.ExtractionUs", 1, 1000 * 1000, 50));
  extraction_histogram.CountMicroseconds(elapsed_time);

  page->url = document.Url();
  page->title = document.title();
  return page;
}

}  // namespace blink
