// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_UPDATER_EXTENSION_UPDATE_DATA_H_
#define EXTENSIONS_BROWSER_UPDATER_EXTENSION_UPDATE_DATA_H_

#include <map>
#include <optional>
#include <string>

#include "base/functional/callback_forward.h"

namespace base {
class Version;
}

namespace extensions {

struct ExtensionUpdateData;
struct ExtensionUpdateCheckParams;

using ExtensionUpdateDataMap = std::map<std::string, ExtensionUpdateData>;

// Called with the extension id and version of the update that was
// found.
using UpdateFoundCallback =
    base::RepeatingCallback<void(const std::string&, const base::Version&)>;

// This struct contains update information for a specific extension.
struct ExtensionUpdateData {
  ExtensionUpdateData();
  ExtensionUpdateData(const ExtensionUpdateData& other);
  ~ExtensionUpdateData();

  std::string install_source;
  bool is_corrupt_reinstall{false};
  std::optional<std::string> pending_version;
  std::optional<std::string> pending_fingerprint;
};

// The basic structure for an extension update check request, which
// can contain a collection of extensions.
struct ExtensionUpdateCheckParams {
  enum UpdateCheckPriority {
    BACKGROUND,
    FOREGROUND,
  };

  ExtensionUpdateCheckParams();
  ExtensionUpdateCheckParams(const ExtensionUpdateCheckParams& other);
  ~ExtensionUpdateCheckParams();

  ExtensionUpdateDataMap update_info;
  UpdateCheckPriority priority;
  bool install_immediately;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_UPDATER_EXTENSION_UPDATE_DATA_H_
