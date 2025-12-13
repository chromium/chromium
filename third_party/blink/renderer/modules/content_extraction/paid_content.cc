// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/content_extraction/paid_content.h"

#include "third_party/blink/renderer/core/dom/container_node.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/static_node_list.h"
#include "third_party/blink/renderer/core/html/html_head_element.h"
#include "third_party/blink/renderer/core/html/html_meta_element.h"
#include "third_party/blink/renderer/core/html/html_script_element.h"
#include "third_party/blink/renderer/platform/json/json_parser.h"
#include "third_party/blink/renderer/platform/json/json_values.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {
namespace {

const char kIsAccessibleForFree[] = "isAccessibleForFree";

bool ObjectValuePresentAndEquals(const JSONObject& object,
                                 const String& key,
                                 const String& value) {
  JSONValue* json_value = object.Get(key);
  if (!json_value) {
    return false;
  }
  if (json_value->GetType() != JSONValue::kTypeString) {
    return false;
  }
  String str_val;
  json_value->AsString(&str_val);
  return str_val == value;
}

bool ObjectValuePresentAndFalse(const JSONObject& object, const String& key) {
  JSONValue* json_value = object.Get(key);
  if (!json_value) {
    return false;
  }

  auto type = json_value->GetType();
  if (type == JSONValue::kTypeString) {
    String str_val;
    json_value->AsString(&str_val);
    if (str_val == "false" || str_val == "False") {
      return true;
    }
    return false;
  }

  bool bool_val;
  json_value->AsBoolean(&bool_val);
  return bool_val == false;
}

// Helper function to parse JSON, with fallbacks for common syntax errors.
std::unique_ptr<JSONValue> ParsePaidContentJSON(const String& json_string) {
  // The JSON provided by some websites has trailing commas, which is not
  // strictly valid JSON. We can allow this by using
  // `ParseJSONWithCommentsDeprecated`.
  JSONParseError error;
  std::unique_ptr<JSONValue> json_value =
      ParseJSONWithCommentsDeprecated(json_string, &error);
  if (!json_value) {
    // The JSON provided by some websites has unescaped newlines in strings,
    // which is not strictly valid JSON. We can work around this by replacing
    // them with spaces.
    String json_text = json_string;
    json_text.Replace('\n', ' ');
    json_value = ParseJSONWithCommentsDeprecated(json_text, &error);
  }
  return json_value;
}
}  // namespace

bool PaidContent::IsPaidElement(const Element* element) const {
  auto* document = &element->GetDocument();
  if (check_microdata_.Contains(document) && check_microdata_.at(document)) {
    for (HTMLMetaElement& meta_element :
         Traversal<HTMLMetaElement>::ChildrenOf(*element)) {
      auto itemprop = meta_element.FastGetAttribute(html_names::kItempropAttr);
      if (itemprop.GetString() != kIsAccessibleForFree) {
        continue;
      }
      return meta_element.Content() == "false";
    }
  }
  for (const auto& paid_element : paid_elements_) {
    if (element == paid_element) {
      return true;
    }
  }
  return false;
}

// Check if the script element is ld+json and has paid content.  Returns the
// script object if paid content is found, and nullptr otherwise.
std::unique_ptr<JSONObject> ScriptHasPaidContent(
    HTMLScriptElement& script_element) {
  ScriptElementBase& script_element_base =
      static_cast<ScriptElementBase&>(script_element);
  if (script_element_base.TypeAttributeValue() != "application/ld+json") {
    return nullptr;
  }
  // The JSON provided by some websites has trailing commas, which is not
  // strictly valid JSON. We can allow this by using
  std::unique_ptr<JSONValue> json_value =
      ParsePaidContentJSON(script_element.textContent());
  if (!json_value || json_value->GetType() != JSONValue::kTypeObject) {
    // JSON parsing failed or it's not an object.
    return nullptr;
  }
  // We know it's an object, so we can safely cast and transfer ownership.
  std::unique_ptr<JSONObject> script_obj = std::unique_ptr<JSONObject>(
      static_cast<JSONObject*>(json_value.release()));

  // check for "schema.org" in "@context"
  JSONValue* context_value = script_obj->Get("@context");
  bool is_context_valid = false;
  if (context_value && context_value->GetType() == JSONValue::kTypeString) {
    String str_val;
    context_value->AsString(&str_val);
    if (str_val.Contains("schema.org")) {
      is_context_valid = true;
    }
  }
  if (!is_context_valid) {
    return nullptr;
  }

  // If we decided to filter for "@type" that should be done here.
  // Supported types are
  // Article, NewsArticle, Blog, Comment, Course, HowTo, Message, Review,
  // and WebPage. Multiple types are supported.

  // check for isAccessibleForFree=false
  if (!ObjectValuePresentAndFalse(*script_obj, kIsAccessibleForFree)) {
    return nullptr;
  };
  return script_obj;
}

bool PaidContent::HasPaidContent(Document& document) {
  // check each ld+json script child of the head element
  const HTMLHeadElement* head = document.head();
  if (head) {
    for (HTMLScriptElement& script_element :
         Traversal<HTMLScriptElement>::ChildrenOf(*head)) {
      if (ScriptHasPaidContent(script_element)) {
        return true;
      }
    }
  }

  return false;
}

bool PaidContent::QueryPaidElements(Document& document) {
  bool paid_content_present = false;

  // check each ld+json script child of the head element
  const HTMLHeadElement* head = document.head();
  if (!head) {
    return paid_content_present;
  }
  for (HTMLScriptElement& script_element :
       Traversal<HTMLScriptElement>::ChildrenOf(*head)) {
    std::unique_ptr<JSONObject> script_obj =
        ScriptHasPaidContent(script_element);
    if (!script_obj) {
      continue;
    }
    paid_content_present = true;

    bool has_part_found = false;

    // Check for hasPart with isAccessibleForFree=false and a cssSelector
    JSONValue* hasPart_val = script_obj->Get("hasPart");
    if (hasPart_val) {
      auto hasPart_type = hasPart_val->GetType();
      if (hasPart_type == JSONValue::kTypeArray) {
        JSONArray* hasPart_array = JSONArray::Cast(hasPart_val);
        for (unsigned j = 0; j < hasPart_array->size(); j++) {
          JSONValue* hasPart_obj_val = hasPart_array->at(j);
          if (hasPart_obj_val->GetType() == JSONValue::kTypeObject) {
            JSONObject* hasPart_obj = JSONObject::Cast(hasPart_obj_val);
            has_part_found |= AppendHasPartElements(document, *hasPart_obj);
          }
        }
      } else if (hasPart_type == JSONValue::kTypeObject) {
        JSONObject* hasPart_obj = JSONObject::Cast(hasPart_val);
        has_part_found |= AppendHasPartElements(document, *hasPart_obj);
      }
    }

    // Assume that pages will only use either ld+json or microdata.
    // If ld+json hasPart exists, don't check for microdata to save
    // the cost of checking each element.
    if (!has_part_found) {
      check_microdata_.Set(&document, true);
    }
    return paid_content_present;
  }
  return paid_content_present;
}

bool PaidContent::AppendHasPartElements(Document& document,
                                        JSONObject& hasPart_obj) {
  if (ObjectValuePresentAndEquals(hasPart_obj, "@type", "WebPageElement") &&
      ObjectValuePresentAndFalse(hasPart_obj, kIsAccessibleForFree)) {
    JSONValue* selector_val = hasPart_obj.Get("cssSelector");
    if (selector_val && selector_val->GetType() == JSONValue::kTypeString) {
      String selector;
      selector_val->AsString(&selector);
      StaticElementList* elements =
          document.QuerySelectorAll(AtomicString(selector));
      if (elements) {
        for (unsigned j = 0; j < elements->length(); j++) {
          paid_elements_.push_back(elements->item(j));
        }
      }
      return true;
    }
  }
  return false;
}
}  // namespace blink
