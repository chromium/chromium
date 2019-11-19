// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_WEBUI_WEB_UI_IOS_DATA_SOURCE_H_
#define IOS_WEB_PUBLIC_WEBUI_WEB_UI_IOS_DATA_SOURCE_H_

#include "base/callback.h"
#include "base/strings/string16.h"
#include "base/supports_user_data.h"

namespace base {
class DictionaryValue;
}

namespace web {
class BrowserState;

// A data source that can help with implementing the common operations needed by
// WebUIIOS pages.
class WebUIIOSDataSource : public base::SupportsUserData {
 public:
  ~WebUIIOSDataSource() override {}

  static WebUIIOSDataSource* Create(const std::string& source_name);

  // Adds a WebUIIOS data source to |browser_state|.
  static void Add(BrowserState* browser_state, WebUIIOSDataSource* source);

  // Adds a string keyed to its name to our dictionary.
  virtual void AddString(const std::string& name,
                         const base::string16& value) = 0;

  // Adds a string keyed to its name to our dictionary.
  virtual void AddString(const std::string& name, const std::string& value) = 0;

  // Adds a localized string with resource |ids| keyed to its name to our
  // dictionary.
  virtual void AddLocalizedString(const std::string& name, int ids) = 0;

  virtual void AddLocalizedStrings(
      const base::DictionaryValue& localized_strings) = 0;

  // Adds a boolean keyed to its name to our dictionary.
  virtual void AddBoolean(const std::string& name, bool value) = 0;

  // Call this to enable a virtual "strings.js" (or "strings.m.js" for modules)
  // URL that provides translations and dynamic data when requested.
  virtual void UseStringsJs() = 0;

  // Adds a mapping between a path name and a resource to return.
  virtual void AddResourcePath(const std::string& path, int resource_id) = 0;

  // Sets the resource to returned when no other paths match.
  virtual void SetDefaultResource(int resource_id) = 0;

  // The following map to methods on URLDataSource. See the documentation there.
  virtual void DisableDenyXFrameOptions() = 0;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_WEBUI_WEB_UI_IOS_DATA_SOURCE_H_
