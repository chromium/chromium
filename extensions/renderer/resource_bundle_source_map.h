// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_RESOURCE_BUNDLE_SOURCE_MAP_H_
#define EXTENSIONS_RENDERER_RESOURCE_BUNDLE_SOURCE_MAP_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "extensions/renderer/source_map.h"
#include "v8/include/v8.h"

namespace ui {
class ResourceBundle;
}

namespace extensions {

class ResourceBundleSourceMap : public SourceMap {
 public:
  explicit ResourceBundleSourceMap(const ui::ResourceBundle* resource_bundle);
  ~ResourceBundleSourceMap() override;

  v8::Local<v8::String> GetSource(v8::Isolate* isolate,
                                  const std::string& name) const override;
  bool Contains(const std::string& name) const override;

  void RegisterSource(const char* const name, int resource_id);

 private:
  struct ResourceInfo {
    ResourceInfo();
    explicit ResourceInfo(int in_id);
    ResourceInfo(ResourceInfo&& other);
    ~ResourceInfo();

    ResourceInfo& operator=(ResourceInfo&& other);

    int id = 0;
    // Used to cache the uncompressed contents if the resource is gzipped.
    mutable std::unique_ptr<std::string> cached;
  };

  const ui::ResourceBundle* resource_bundle_;
  std::map<std::string, ResourceInfo> resource_map_;

  DISALLOW_COPY_AND_ASSIGN(ResourceBundleSourceMap);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_RESOURCE_BUNDLE_SOURCE_MAP_H_
