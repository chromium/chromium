// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_MANIFEST_HANDLERS_OPTIONS_PAGE_INFO_H_
#define EXTENSIONS_COMMON_MANIFEST_HANDLERS_OPTIONS_PAGE_INFO_H_

#include <string>
#include <vector>

#include "base/values.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handler.h"
#include "url/gurl.h"

namespace base {
class Value;
}

namespace extensions {

// A class to provide options page configuration settings from the manifest.
class OptionsPageInfo : public Extension::ManifestData {
 public:
  OptionsPageInfo(const GURL& options_page,
                  bool chrome_styles,
                  bool open_in_tab);

  OptionsPageInfo(const OptionsPageInfo&) = delete;
  OptionsPageInfo& operator=(const OptionsPageInfo&) = delete;

  ~OptionsPageInfo() override;

  // Returns the URL to the given extension's options page. This method supports
  // both the "options_ui.page" field and the legacy "options_page" field. If
  // both are present, it will return the value of "options_ui.page".
  static const GURL& GetOptionsPage(const Extension* extension);

  // Returns true if the given extension has an options page. An extension has
  // an options page if one or both of "options_ui.page" and "options_page"
  // specify a valid options page.
  static bool HasOptionsPage(const Extension* extension);

  // Returns whether the Chrome user agent stylesheet should be applied to the
  // given extension's options page.
  static bool ShouldUseChromeStyle(const Extension* extension);

  // Returns whether the given extension's options page should be opened in a
  // new tab instead of an embedded popup.
  static bool ShouldOpenInTab(const Extension* extension);

  static std::unique_ptr<OptionsPageInfo> Create(
      Extension* extension,
      const base::Value::Dict* options_ui_dict,
      const std::string& options_page_string,
      std::vector<InstallWarning>* install_warnings,
      std::u16string* error);

 private:
  // The URL to the options page of this extension. We only store one options
  // URL, either options_page or options_ui.page. options_ui.page is preferred
  // if both are present.
  GURL options_page_;

  bool chrome_styles_;

  bool open_in_tab_;
};

// Parses the "options_ui" manifest key and the legacy "options_page" key.
class OptionsPageHandler : public ManifestHandler {
 public:
  OptionsPageHandler();

  OptionsPageHandler(const OptionsPageHandler&) = delete;
  OptionsPageHandler& operator=(const OptionsPageHandler&) = delete;

  ~OptionsPageHandler() override;

  bool Parse(Extension* extension, std::u16string* error) override;
  bool Validate(const Extension* extension,
                std::string* error,
                std::vector<InstallWarning>* warnings) const override;

 private:
  base::span<const char* const> Keys() const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_MANIFEST_HANDLERS_OPTIONS_PAGE_INFO_H_
