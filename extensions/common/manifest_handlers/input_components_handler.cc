// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/manifest_handlers/input_components_handler.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/options_page_info.h"

namespace extensions {

namespace keys = manifest_keys;
namespace errors = manifest_errors;

InputComponentInfo::InputComponentInfo() = default;

InputComponentInfo::InputComponentInfo(const InputComponentInfo& other) =
    default;

InputComponentInfo::~InputComponentInfo() = default;

InputComponents::InputComponents() = default;
InputComponents::~InputComponents() = default;

// static
const std::vector<InputComponentInfo>* InputComponents::GetInputComponents(
    const Extension* extension) {
  InputComponents* info = static_cast<InputComponents*>(
      extension->GetManifestData(keys::kInputComponents));
  return info ? &info->input_components : nullptr;
}

InputComponentsHandler::InputComponentsHandler() = default;

InputComponentsHandler::~InputComponentsHandler() = default;

bool InputComponentsHandler::Parse(Extension* extension,
                                   std::u16string* error) {
  const base::Value* list_value;
  if (!extension->manifest()->GetList(keys::kInputComponents, &list_value)) {
    *error = errors::kInvalidInputComponents16;
    return false;
  }

  auto info = std::make_unique<InputComponents>();
  for (size_t i = 0; i < list_value->GetList().size(); ++i) {
    const base::Value::Dict* module_value =
        list_value->GetList()[i].GetIfDict();
    if (!module_value) {
      *error = errors::kInvalidInputComponents16;
      return false;
    }

    // Get input_components[i].name.
    const std::string* name_str = module_value->FindString(keys::kName);
    if (!name_str) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidInputComponentName, base::NumberToString(i));
      return false;
    }

    // Get input_components[i].id.
    std::string id_str;
    const std::string* maybe_id_str = module_value->FindString(keys::kId);
    if (maybe_id_str) {
      id_str = *maybe_id_str;
    }

    // Get input_components[i].language.
    // Both string and list of string are allowed to be compatibile with old
    // input_ime manifest specification.
    std::set<std::string> languages;
    const base::Value* language_value = module_value->Find(keys::kLanguage);
    if (language_value) {
      if (language_value->is_string()) {
        languages.insert(language_value->GetString());
      } else if (language_value->is_list()) {
        for (const auto& language : language_value->GetList()) {
          if (language.is_string())
            languages.insert(language.GetString());
        }
      }
    }

    std::set<std::string> layouts;
    const base::Value::List* layouts_value =
        module_value->FindList(keys::kLayouts);
    if (layouts_value) {
      for (size_t j = 0; j < layouts_value->size(); ++j) {
        const base::Value& layout = (*layouts_value)[j];
        if (!layout.is_string()) {
          *error = ErrorUtils::FormatErrorMessageUTF16(
              errors::kInvalidInputComponentLayoutName, base::NumberToString(i),
              base::NumberToString(j));
          return false;
        }
        layouts.insert(layout.GetString());
      }
    }

    // Get input_components[i].input_view_url.
    // Note: 'input_view' is optional in manifest.
    GURL input_view_url;
    const std::string* input_view_str =
        module_value->FindString(keys::kInputView);
    if (input_view_str) {
      input_view_url = extension->GetResourceURL(*input_view_str);
      if (!input_view_url.is_valid()) {
        *error = ErrorUtils::FormatErrorMessageUTF16(errors::kInvalidInputView,
                                                     base::NumberToString(i));
        return false;
      }
    }

    // Get input_components[i].options_page_url.
    // Note: 'options_page' is optional in manifest.
    GURL options_page_url;
    const std::string* options_page_str =
        module_value->FindString(keys::kImeOptionsPage);
    if (options_page_str) {
      options_page_url = extension->GetResourceURL(*options_page_str);
      if (!options_page_url.is_valid()) {
        *error = ErrorUtils::FormatErrorMessageUTF16(
            errors::kInvalidOptionsPage, base::NumberToString(i));
        return false;
      }
    } else {
      // Fall back to extension's options page for backward compatibility.
      options_page_url = extensions::OptionsPageInfo::GetOptionsPage(extension);
    }

    InputComponentInfo component;
    component.name = *name_str;
    component.id = std::move(id_str);
    component.languages = std::move(languages);
    component.layouts = std::move(layouts);
    component.options_page_url = std::move(options_page_url);
    component.input_view_url = std::move(input_view_url);
    info->input_components.push_back(std::move(component));
  }
  extension->SetManifestData(keys::kInputComponents, std::move(info));
  return true;
}

const std::vector<std::string>
InputComponentsHandler::PrerequisiteKeys() const {
  return SingleKey(keys::kOptionsPage);
}

base::span<const char* const> InputComponentsHandler::Keys() const {
  static constexpr const char* kKeys[] = {keys::kInputComponents};
  return kKeys;
}

}  // namespace extensions
