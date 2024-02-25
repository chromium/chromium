// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_ERROR_MAP_H_
#define EXTENSIONS_BROWSER_ERROR_MAP_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/containers/circular_deque.h"
#include "extensions/browser/extension_error.h"
#include "extensions/common/extension_id.h"

namespace extensions {

using ErrorList = base::circular_deque<std::unique_ptr<ExtensionError>>;

// An ErrorMap is responsible for storing Extension-related errors, keyed by
// Extension ID.
class ErrorMap {
 public:
  ErrorMap();

  ErrorMap(const ErrorMap&) = delete;
  ErrorMap& operator=(const ErrorMap&) = delete;

  ~ErrorMap();

  struct Filter {
    Filter(const ExtensionId& restrict_to_extension_id,
           int restrict_to_type,
           const std::set<int>& restrict_to_ids,
           bool restrict_to_incognito);
    Filter(const Filter& other);
    ~Filter();

    // Convenience methods to get a specific type of filter. Prefer these over
    // the constructor when possible.
    static Filter ErrorsForExtension(const ExtensionId& extension_id);
    static Filter ErrorsForExtensionWithType(const ExtensionId& extension_id,
                                             ExtensionError::Type type);
    static Filter ErrorsForExtensionWithIds(const ExtensionId& extension_id,
                                            const std::set<int>& ids);
    static Filter ErrorsForExtensionWithTypeAndIds(
        const ExtensionId& extension_id,
        ExtensionError::Type type,
        const std::set<int>& ids);
    static Filter IncognitoErrors();

    bool Matches(const ExtensionError* error) const;

    const ExtensionId restrict_to_extension_id;
    const int restrict_to_type;
    const std::set<int> restrict_to_ids;
    const bool restrict_to_incognito;
  };

  // Return the list of all errors associated with the given extension.
  const ErrorList& GetErrorsForExtension(const ExtensionId& extension_id) const;

  // Add the |error| to the ErrorMap.
  const ExtensionError* AddError(std::unique_ptr<ExtensionError> error);

  // Removes errors that match the given |filter| from the map. If non-null,
  // |affected_ids| will be populated with the set of extension ids that were
  // affected by this removal.
  void RemoveErrors(const Filter& filter, std::set<ExtensionId>* affected_ids);

  // Remove all errors for all extensions, and clear the map.
  void RemoveAllErrors();

  size_t size() const { return map_.size(); }

 private:
  // An Entry is created for each Extension ID, and stores the errors related to
  // that Extension.
  class ExtensionEntry;

  // The mapping between Extension IDs and their corresponding Entries.
  std::map<ExtensionId, std::unique_ptr<ExtensionEntry>> map_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_ERROR_MAP_H_
