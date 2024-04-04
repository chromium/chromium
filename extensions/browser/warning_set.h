// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_WARNING_SET_H_
#define EXTENSIONS_BROWSER_WARNING_SET_H_

#include <set>
#include <string>
#include <vector>

#include "extensions/common/extension_id.h"

namespace base {
class FilePath;
}

namespace extensions {

class ExtensionSet;

// This class is used by the WarningService to represent warnings if extensions
// misbehave. Note that the WarningService deals only with specific warnings
// that should trigger a badge on the Chrome menu button.
class Warning {
 public:
  enum WarningType {
    // Don't use this, it is only intended for the default constructor and
    // does not have localized warning messages for the UI.
    kInvalid = 0,
    // An extension caused excessive network delays.
    kNetworkDelay,
    // The extension repeatedly flushed WebKit's in-memory cache, which slows
    // down the overall performance.
    kRepeatedCacheFlushes,
    // The extension failed to determine the filename of a download because
    // another extension with higher precedence determined a different filename.
    kDownloadFilenameConflict,
    kReloadTooFrequent,
    // The declarative net request ruleset for the extension failed to load.
    kRulesetFailedToLoad,
    // The declarative net request ruleset for the extension could not be
    // enabled because it would exceed the global rules limit.
    kEnabledRuleCountExceeded,
    kMaxWarningType
  };

  // We allow copy&assign for passing containers of Warnings between threads.
  Warning(const Warning& other);
  ~Warning();
  Warning& operator=(const Warning& other);

  // Factory methods for various warning types.
  static Warning CreateNetworkDelayWarning(const ExtensionId& extension_id);
  static Warning CreateRepeatedCacheFlushesWarning(
      const ExtensionId& extension_id);
  static Warning CreateDownloadFilenameConflictWarning(
      const std::string& losing_extension_id,
      const std::string& winning_extension_id,
      const base::FilePath& losing_filename,
      const base::FilePath& winning_filename);
  static Warning CreateReloadTooFrequentWarning(
      const ExtensionId& extension_id);
  static Warning CreateRulesetFailedToLoadWarning(
      const ExtensionId& extension_id);
  static Warning CreateEnabledRuleCountExceededWarning(
      const ExtensionId& extension_id);

  // Compare Warnings based on the tuple of (extension_id, type).
  // The message associated with Warnings is purely informational
  // and does not contribute to distinguishing extensions.
  bool operator<(const Warning& other) const;

  // Returns the specific warning type.
  WarningType warning_type() const { return type_; }

  // Returns the id of the extension for which this warning is valid.
  const ExtensionId& extension_id() const { return extension_id_; }

  // Returns a localized warning message.
  std::string GetLocalizedMessage(const ExtensionSet* extensions) const;

 private:
  // Constructs a warning of type |type| for extension |extension_id|. This
  // could indicate for example the fact that an extension conflicted with
  // others. The |message_id| refers to an IDS_ string ID. The
  // |message_parameters| are filled into the message template.
  Warning(WarningType type,
          const ExtensionId& extension_id,
          int message_id,
          const std::vector<std::string>& message_parameters);

  WarningType type_;
  ExtensionId extension_id_;
  // IDS_* resource ID.
  int message_id_;
  // Parameters to be filled into the string identified by |message_id_|.
  std::vector<std::string> message_parameters_;
};

typedef std::set<Warning> WarningSet;

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_WARNING_SET_H_
