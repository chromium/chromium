// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/extension_api.h"

#include <stddef.h>

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "extensions/common/extension.h"
#include "extensions/common/extensions_client.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/common/features/simple_feature.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "url/gurl.h"

namespace extensions {

namespace {

const char* const kChildKinds[] = {"functions", "events"};

base::Value::Dict LoadSchemaDictionary(const std::string& name,
                                       std::string_view schema) {
  auto result = base::JSONReader::ReadAndReturnValueWithError(schema);

  // Tracking down http://crbug.com/121424
  char buf[128];
  base::snprintf(buf, std::size(buf), "%s: (%d) '%s'", name.c_str(),
                 result.has_value() ? static_cast<int>(result->type()) : -1,
                 !result.has_value() ? result.error().message.c_str() : "");

  CHECK(result.has_value())
      << result.error().message << " for schema " << schema;
  CHECK(result->is_dict()) << " for schema " << schema;
  return std::move(*result).TakeDict();
}

const base::Value::Dict* FindListItem(const base::Value::List& list,
                                      const std::string& property_name,
                                      const std::string& property_value) {
  for (const base::Value& item_value : list) {
    const base::Value::Dict* item = item_value.GetIfDict();
    CHECK(item) << property_value << "/" << property_name;
    const std::string* value = item->FindStringByDottedPath(property_name);
    if (value && *value == property_value) {
      return item;
    }
  }

  return nullptr;
}

const base::Value::Dict* GetSchemaChild(const base::Value::Dict& schema_node,
                                        const std::string& child_name) {
  for (const char* kind : kChildKinds) {
    const base::Value::List* list_node = schema_node.FindList(kind);
    if (!list_node) {
      continue;
    }
    const base::Value::Dict* child_node =
        FindListItem(*list_node, "name", child_name);
    if (child_node) {
      return child_node;
    }
  }

  return nullptr;
}

struct ExtensionAPIStatic {
  ExtensionAPIStatic() : api(ExtensionAPI::CreateWithDefaultConfiguration()) {}
  std::unique_ptr<ExtensionAPI> api;
};

base::LazyInstance<ExtensionAPIStatic>::Leaky g_extension_api_static =
    LAZY_INSTANCE_INITIALIZER;

// May override |g_extension_api_static| for a test.
ExtensionAPI* g_shared_instance_for_test = nullptr;

}  // namespace

// static
ExtensionAPI* ExtensionAPI::GetSharedInstance() {
  return g_shared_instance_for_test ? g_shared_instance_for_test
                                    : g_extension_api_static.Get().api.get();
}

// static
ExtensionAPI* ExtensionAPI::CreateWithDefaultConfiguration() {
  ExtensionAPI* api = new ExtensionAPI();
  api->InitDefaultConfiguration();
  return api;
}

// static
void ExtensionAPI::SplitDependencyName(const std::string& full_name,
                                       std::string* feature_type,
                                       std::string* feature_name) {
  size_t colon_index = full_name.find(':');
  if (colon_index == std::string::npos) {
    // TODO(aa): Remove this code when all API descriptions have been updated.
    *feature_type = "api";
    *feature_name = full_name;
    return;
  }

  *feature_type = full_name.substr(0, colon_index);
  *feature_name = full_name.substr(colon_index + 1);
}

ExtensionAPI::OverrideSharedInstanceForTest::OverrideSharedInstanceForTest(
    ExtensionAPI* testing_api)
    : original_api_(g_shared_instance_for_test) {
  g_shared_instance_for_test = testing_api;
}

ExtensionAPI::OverrideSharedInstanceForTest::~OverrideSharedInstanceForTest() {
  g_shared_instance_for_test = original_api_;
}

void ExtensionAPI::LoadSchema(const std::string& name,
                              std::string_view schema) {
  lock_.AssertAcquired();
  base::Value::Dict schema_dict(LoadSchemaDictionary(name, schema));
  const std::string* schema_namespace = schema_dict.FindString("namespace");
  CHECK(schema_namespace);
  schemas_[*schema_namespace] = std::move(schema_dict);
}

ExtensionAPI::ExtensionAPI() = default;

ExtensionAPI::~ExtensionAPI() = default;

void ExtensionAPI::InitDefaultConfiguration() {
  static const constexpr char* const names[] = {"api", "behavior", "manifest",
                                                "permission"};
  for (const char* const name : names) {
    RegisterDependencyProvider(name, FeatureProvider::GetByName(name));
  }

  default_configuration_initialized_ = true;
}

void ExtensionAPI::RegisterDependencyProvider(const std::string& name,
                                              const FeatureProvider* provider) {
  dependency_providers_[name] = provider;
}

bool ExtensionAPI::IsAnyFeatureAvailableToContext(
    const Feature& api,
    const Extension* extension,
    mojom::ContextType context,
    const GURL& url,
    CheckAliasStatus check_alias,
    int context_id,
    const ContextData& context_data) {
  auto provider = dependency_providers_.find("api");
  CHECK(provider != dependency_providers_.end());

  if (api.IsAvailableToContext(extension, context, url, context_id,
                               context_data)
          .is_available()) {
    return true;
  }

  // Check to see if there are any parts of this API that are allowed in this
  // context.
  const std::vector<const Feature*> features =
      provider->second->GetChildren(api);
  for (const Feature* feature : features) {
    if (feature
            ->IsAvailableToContext(extension, context, url, context_id,
                                   context_data)
            .is_available()) {
      return true;
    }
  }

  if (check_alias != CheckAliasStatus::ALLOWED) {
    return false;
  }

  const std::string& alias_name = api.alias();
  if (alias_name.empty()) {
    return false;
  }

  const Feature* alias = provider->second->GetFeature(alias_name);
  CHECK(alias) << "Cannot find alias feature " << alias_name
               << " for API feature " << api.name();
  return IsAnyFeatureAvailableToContext(*alias, extension, context, url,
                                        CheckAliasStatus::NOT_ALLOWED,
                                        context_id, context_data);
}

Feature::Availability ExtensionAPI::IsAvailable(
    const std::string& full_name,
    const Extension* extension,
    mojom::ContextType context,
    const GURL& url,
    CheckAliasStatus check_alias,
    int context_id,
    const ContextData& context_data) {
  const Feature* feature = GetFeatureDependency(full_name);
  if (!feature) {
    return Feature::Availability(Feature::NOT_PRESENT,
                                 std::string("Unknown feature: ") + full_name);
  }

  Feature::Availability availability = feature->IsAvailableToContext(
      extension, context, url, context_id, context_data);
  if (availability.is_available() || check_alias != CheckAliasStatus::ALLOWED) {
    return availability;
  }

  Feature::Availability alias_availability = IsAliasAvailable(
      full_name, *feature, extension, context, url, context_id, context_data);
  return alias_availability.is_available() ? alias_availability : availability;
}

std::string_view ExtensionAPI::GetSchemaStringPiece(
    const std::string& api_name) {
  base::AutoLock lock(lock_);
  return GetSchemaStringPieceUnsafe(api_name);
}

const base::Value::Dict* ExtensionAPI::GetSchema(const std::string& full_name) {
  base::AutoLock lock(lock_);
  std::string child_name;
  std::string api_name = GetAPINameFromFullNameUnsafe(full_name, &child_name);

  const base::Value::Dict* result = nullptr;
  auto maybe_schema = schemas_.find(api_name);
  if (maybe_schema != schemas_.end()) {
    result = &maybe_schema->second;
  } else {
    std::string_view schema_string = GetSchemaStringPieceUnsafe(api_name);
    if (schema_string.empty()) {
      return nullptr;
    }
    LoadSchema(api_name, schema_string);

    maybe_schema = schemas_.find(api_name);
    CHECK(schemas_.end() != maybe_schema);
    result = &maybe_schema->second;
  }

  if (!child_name.empty()) {
    result = GetSchemaChild(*result, child_name);
  }

  return result;
}

const Feature* ExtensionAPI::GetFeatureDependency(
    const std::string& full_name) {
  std::string feature_type;
  std::string feature_name;
  SplitDependencyName(full_name, &feature_type, &feature_name);

  auto provider = dependency_providers_.find(feature_type);
  if (provider == dependency_providers_.end()) {
    return nullptr;
  }

  const Feature* feature = provider->second->GetFeature(feature_name);
  // Try getting the feature for the parent API, if this was a child.
  if (!feature) {
    std::string child_name;
    feature = provider->second->GetFeature(
        GetAPINameFromFullName(feature_name, &child_name));
  }
  return feature;
}

std::string ExtensionAPI::GetAPINameFromFullName(const std::string& full_name,
                                                 std::string* child_name) {
  base::AutoLock lock(lock_);
  return GetAPINameFromFullNameUnsafe(full_name, child_name);
}

bool ExtensionAPI::IsKnownAPI(const std::string& name,
                              ExtensionsClient* client) {
  lock_.AssertAcquired();
  return base::Contains(schemas_, name) || client->IsAPISchemaGenerated(name);
}

Feature::Availability ExtensionAPI::IsAliasAvailable(
    const std::string& full_name,
    const Feature& feature,
    const Extension* extension,
    mojom::ContextType context,
    const GURL& url,
    int context_id,
    const ContextData& context_data) {
  const std::string& alias = feature.alias();
  if (alias.empty()) {
    return Feature::Availability(Feature::NOT_PRESENT, "Alias not defined");
  }

  auto provider = dependency_providers_.find("api");
  CHECK(provider != dependency_providers_.end());

  // Check if there is a child feature associated with full name for alias API.
  // This is to ensure that the availability of the feature associated with the
  // aliased |full_name| is properly determined in case the feature in question
  // is a child feature. For example, if API foo has an alias fooAlias, which
  // has a child feature fooAlias.method, aliased foo.method availability should
  // be determined using fooAlias.method feature, rather than fooAlias feature.
  std::string child_name;
  GetAPINameFromFullName(full_name, &child_name);
  std::string full_alias_name = alias + "." + child_name;
  const Feature* alias_feature = provider->second->GetFeature(full_alias_name);

  // If there is no child feature, use the alias API feature to check
  // availability.
  if (!alias_feature) {
    alias_feature = provider->second->GetFeature(alias);
  }

  CHECK(alias_feature) << "Cannot find alias feature " << alias
                       << " for API feature " << feature.name();

  return alias_feature->IsAvailableToContext(extension, context, url,
                                             context_id, context_data);
}

std::string_view ExtensionAPI::GetSchemaStringPieceUnsafe(
    const std::string& api_name) {
  lock_.AssertAcquired();
  DCHECK_EQ(api_name, GetAPINameFromFullNameUnsafe(api_name, nullptr));
  ExtensionsClient* client = ExtensionsClient::Get();
  DCHECK(client);
  if (!default_configuration_initialized_) {
    return std::string_view();
  }

  return client->GetAPISchema(api_name);
}

std::string ExtensionAPI::GetAPINameFromFullNameUnsafe(
    const std::string& full_name,
    std::string* child_name) {
  lock_.AssertAcquired();
  std::string api_name_candidate = full_name;
  ExtensionsClient* extensions_client = ExtensionsClient::Get();
  DCHECK(extensions_client);
  while (true) {
    if (IsKnownAPI(api_name_candidate, extensions_client)) {
      if (child_name) {
        if (api_name_candidate.length() < full_name.length()) {
          *child_name = full_name.substr(api_name_candidate.length() + 1);
        } else {
          *child_name = "";
        }
      }
      return api_name_candidate;
    }

    size_t last_dot_index = api_name_candidate.rfind('.');
    if (last_dot_index == std::string::npos) {
      break;
    }

    api_name_candidate = api_name_candidate.substr(0, last_dot_index);
  }

  if (child_name) {
    *child_name = "";
  }
  return std::string();
}

}  // namespace extensions
