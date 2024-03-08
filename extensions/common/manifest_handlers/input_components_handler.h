// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_MANIFEST_HANDLERS_INPUT_COMPONENTS_HANDLER_H_
#define EXTENSIONS_COMMON_MANIFEST_HANDLERS_INPUT_COMPONENTS_HANDLER_H_

#include <set>
#include <string>
#include <vector>

#include "extensions/common/extension.h"
#include "extensions/common/manifest_handler.h"
#include "url/gurl.h"

namespace extensions {

class Extension;

struct InputComponentInfo {
  // Define out of line constructor/destructor to please Clang.
  InputComponentInfo();
  InputComponentInfo(const InputComponentInfo& other);
  ~InputComponentInfo();

  std::string name;
  std::string id;
  std::set<std::string> languages;
  std::set<std::string> layouts;
  GURL options_page_url;
  GURL input_view_url;
};

struct InputComponents : public Extension::ManifestData {
  // Define out of line constructor/destructor to please Clang.
  InputComponents();
  ~InputComponents() override;

  std::vector<InputComponentInfo> input_components;

  // Returns list of input components and associated properties.
  static const std::vector<InputComponentInfo>* GetInputComponents(
      const Extension* extension);
};

// Parses the "incognito" manifest key.
class InputComponentsHandler : public ManifestHandler {
 public:
  InputComponentsHandler();
  InputComponentsHandler(const InputComponentsHandler&) = delete;
  InputComponentsHandler& operator=(const InputComponentsHandler&) = delete;
  ~InputComponentsHandler() override;

  bool Parse(Extension* extension, std::u16string* error) override;

  // Requires kOptionsPage is already parsed.
  const std::vector<std::string> PrerequisiteKeys() const override;

 private:
  base::span<const char* const> Keys() const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_MANIFEST_HANDLERS_INPUT_COMPONENTS_HANDLER_H_
