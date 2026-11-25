// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/resource_bundle_source_map.h"

#include <algorithm>
#include <functional>
#include <ostream>
#include <string>
#include <string_view>

#include "base/check.h"
#include "base/notreached.h"
#include "extensions/renderer/v8_helpers.h"
#include "ui/base/resource/resource_bundle.h"
#include "v8/include/v8-primitive.h"

namespace extensions {

ResourceBundleSourceMap::ResourceBundleSourceMap(
    const ui::ResourceBundle* resource_bundle)
    : resource_bundle_(resource_bundle) {}

ResourceBundleSourceMap::~ResourceBundleSourceMap() = default;

v8::Local<v8::String> ResourceBundleSourceMap::GetSource(
    v8::Isolate* isolate,
    std::string_view name) const {
  std::optional<int> resource_id;
  {
    base::AutoLock lock(lock_);
    resource_id = FindResourceId(name);
  }
  if (!resource_id) {
    DUMP_WILL_BE_NOTREACHED()
        << "No module is registered with name \"" << name << "\"";
    return v8::Local<v8::String>();
  }

  std::string resource = resource_bundle_->LoadDataResourceString(*resource_id);
  if (resource.empty()) {
    DUMP_WILL_BE_NOTREACHED()
        << "Module resource registered as \"" << name << "\" not found";
    return v8::Local<v8::String>();
  }

  v8::Local<v8::String> result;
  CHECK(v8_helpers::ToV8String(isolate, resource, &result));
  return result;
}

bool ResourceBundleSourceMap::Contains(std::string_view name) const {
  base::AutoLock lock(lock_);
  return FindResourceId(name).has_value();
}

std::optional<int> ResourceBundleSourceMap::FindResourceId(
    std::string_view name) const {
  for (const SourceTable& table : source_tables_) {
    auto source = std::ranges::lower_bound(table, name, std::less<>(),
                                           &SourceEntry::first);
    if (source != table.end() && source->first == name) {
      return source->second;
    }
  }
  return std::nullopt;
}

}  // namespace extensions
