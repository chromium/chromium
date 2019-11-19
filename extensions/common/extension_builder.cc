// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/extension_builder.h"

#include <utility>

#include "base/optional.h"
#include "components/crx_file/id_util.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"

namespace extensions {

constexpr char ExtensionBuilder::kServiceWorkerScriptFile[];

struct ExtensionBuilder::ManifestData {
  Type type;
  std::string name;
  std::vector<std::string> permissions;
  base::Optional<ActionType> action;
  base::Optional<BackgroundContext> background_context;
  base::Optional<std::string> version;

  // A ContentScriptEntry includes a string name, and a vector of string
  // match patterns.
  using ContentScriptEntry = std::pair<std::string, std::vector<std::string>>;
  std::vector<ContentScriptEntry> content_scripts;

  base::Optional<base::Value> extra;

  std::unique_ptr<base::DictionaryValue> GetValue() const {
    DictionaryBuilder manifest;
    manifest.Set(manifest_keys::kName, name)
        .Set(manifest_keys::kManifestVersion, 2)
        .Set(manifest_keys::kVersion, version.value_or("0.1"))
        .Set(manifest_keys::kDescription, "some description");

    switch (type) {
      case Type::EXTENSION:
        break;  // Sufficient already.
      case Type::PLATFORM_APP: {
        DictionaryBuilder background;
        background.Set("scripts", ListBuilder().Append("test.js").Build());
        manifest.Set(
            "app",
            DictionaryBuilder().Set("background", background.Build()).Build());
        break;
      }
    }

    if (!permissions.empty()) {
      ListBuilder permissions_builder;
      for (const std::string& permission : permissions)
        permissions_builder.Append(permission);
      manifest.Set(manifest_keys::kPermissions, permissions_builder.Build());
    }

    if (action) {
      const char* action_key = nullptr;
      switch (*action) {
        case ActionType::PAGE_ACTION:
          action_key = manifest_keys::kPageAction;
          break;
        case ActionType::BROWSER_ACTION:
          action_key = manifest_keys::kBrowserAction;
          break;
      }
      manifest.Set(action_key, std::make_unique<base::DictionaryValue>());
    }

    if (background_context) {
      DictionaryBuilder background;
      base::Optional<bool> persistent;
      switch (*background_context) {
        case BackgroundContext::BACKGROUND_PAGE:
          background.Set("page", "background_page.html");
          persistent = true;
          break;
        case BackgroundContext::EVENT_PAGE:
          background.Set("page", "background_page.html");
          persistent = false;
          break;
        case BackgroundContext::SERVICE_WORKER:
          background.Set("service_worker", kServiceWorkerScriptFile);
          break;
      }
      if (persistent) {
        background.Set("persistent", *persistent);
      }
      manifest.Set("background", background.Build());
    }

    if (!content_scripts.empty()) {
      ListBuilder scripts_value;
      for (const auto& script : content_scripts) {
        ListBuilder matches;
        for (const auto& match : script.second)
          matches.Append(match);
        scripts_value.Append(
            DictionaryBuilder()
                .Set(manifest_keys::kJs,
                     ListBuilder().Append(script.first).Build())
                .Set(manifest_keys::kMatches, matches.Build())
                .Build());
      }
      manifest.Set(manifest_keys::kContentScripts, scripts_value.Build());
    }

    std::unique_ptr<base::DictionaryValue> result = manifest.Build();
    if (extra) {
      const base::DictionaryValue* extra_dict = nullptr;
      extra->GetAsDictionary(&extra_dict);
      result->MergeDictionary(extra_dict);
    }

    return result;
  }

  base::Value* get_extra() {
    if (!extra)
      extra.emplace(base::Value::Type::DICTIONARY);
    return &extra.value();
  }
};

ExtensionBuilder::ExtensionBuilder()
    : location_(Manifest::UNPACKED), flags_(Extension::NO_FLAGS) {}

ExtensionBuilder::ExtensionBuilder(const std::string& name, Type type)
    : ExtensionBuilder() {
  manifest_data_ = std::make_unique<ManifestData>();
  manifest_data_->name = name;
  manifest_data_->type = type;
}

ExtensionBuilder::~ExtensionBuilder() {}

ExtensionBuilder::ExtensionBuilder(ExtensionBuilder&& other) = default;
ExtensionBuilder& ExtensionBuilder::operator=(ExtensionBuilder&& other) =
    default;

scoped_refptr<const Extension> ExtensionBuilder::Build() {
  CHECK(manifest_data_ || manifest_value_);

  if (id_.empty() && manifest_data_)
    id_ = crx_file::id_util::GenerateId(manifest_data_->name);

  std::string error;
  scoped_refptr<const Extension> extension = Extension::Create(
      path_, location_,
      manifest_data_ ? *manifest_data_->GetValue() : *manifest_value_, flags_,
      id_, &error);

  CHECK(error.empty()) << error;
  CHECK(extension);

  return extension;
}

ExtensionBuilder& ExtensionBuilder::AddPermission(
    const std::string& permission) {
  CHECK(manifest_data_);
  manifest_data_->permissions.push_back(permission);
  return *this;
}

ExtensionBuilder& ExtensionBuilder::AddPermissions(
    const std::vector<std::string>& permissions) {
  CHECK(manifest_data_);
  manifest_data_->permissions.insert(manifest_data_->permissions.end(),
                                     permissions.begin(), permissions.end());
  return *this;
}

ExtensionBuilder& ExtensionBuilder::SetAction(ActionType action) {
  CHECK(manifest_data_);
  manifest_data_->action = action;
  return *this;
}

ExtensionBuilder& ExtensionBuilder::SetBackgroundContext(
    BackgroundContext background_context) {
  CHECK(manifest_data_);
  manifest_data_->background_context = background_context;
  return *this;
}

ExtensionBuilder& ExtensionBuilder::AddContentScript(
    const std::string& script_name,
    const std::vector<std::string>& match_patterns) {
  CHECK(manifest_data_);
  manifest_data_->content_scripts.emplace_back(script_name, match_patterns);
  return *this;
}

ExtensionBuilder& ExtensionBuilder::SetVersion(const std::string& version) {
  CHECK(manifest_data_);
  manifest_data_->version = version;
  return *this;
}

ExtensionBuilder& ExtensionBuilder::SetPath(const base::FilePath& path) {
  path_ = path;
  return *this;
}

ExtensionBuilder& ExtensionBuilder::SetLocation(Manifest::Location location) {
  location_ = location;
  return *this;
}

ExtensionBuilder& ExtensionBuilder::SetManifest(
    std::unique_ptr<base::DictionaryValue> manifest) {
  CHECK(!manifest_data_);
  manifest_value_ = std::move(manifest);
  return *this;
}

ExtensionBuilder& ExtensionBuilder::MergeManifest(
    std::unique_ptr<base::DictionaryValue> manifest) {
  if (manifest_data_) {
    base::DictionaryValue* extra_dict = nullptr;
    manifest_data_->get_extra()->GetAsDictionary(&extra_dict);
    extra_dict->MergeDictionary(manifest.get());
  } else {
    manifest_value_->MergeDictionary(manifest.get());
  }
  return *this;
}

ExtensionBuilder& ExtensionBuilder::AddFlags(int init_from_value_flags) {
  flags_ |= init_from_value_flags;
  return *this;
}

ExtensionBuilder& ExtensionBuilder::SetID(const std::string& id) {
  id_ = id;
  return *this;
}

void ExtensionBuilder::SetManifestKeyImpl(base::StringPiece key,
                                          base::Value value) {
  CHECK(manifest_data_);
  manifest_data_->get_extra()->SetKey(key, std::move(value));
}

void ExtensionBuilder::SetManifestPathImpl(
    std::initializer_list<base::StringPiece> path,
    base::Value value) {
  CHECK(manifest_data_);
  manifest_data_->get_extra()->SetPath(path, std::move(value));
}

}  // namespace extensions
