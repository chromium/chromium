// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_COMMON_MANIFEST_ID_H_
#define COMPONENTS_WEBAPPS_COMMON_MANIFEST_ID_H_

#include <compare>
#include <iosfwd>
#include <optional>
#include <string_view>
#include <utility>

#include "url/gurl.h"

namespace webapps {

// A strong type for the Manifest ID of a web app.
class ValidManifestId {
 public:
  // Creates a manifest 'id' from the `url_str`, by parsing it as a url and then
  // following the manifest `id` parsing algorithm at
  // https://www.w3.org/TR/appmanifest/#id-member, where it strips any
  // #fragments from the url. Note that `url_str` must be an absolute URL,
  // as the GURL constructor will fail for relative URLs without a base.
  static std::optional<ValidManifestId> Create(std::string_view url_str);

  // Creates a manifest 'id' from the `url`. This follows the manifest `id`
  // parsing
  // algorithm at https://www.w3.org/TR/appmanifest/#id-member, and strips any
  // #fragments from the url.
  static std::optional<ValidManifestId> Create(const GURL& url);

  // Creates a manifest 'id' from the `url`. This follows the manifest `id`
  // parsing algorithm at https://www.w3.org/TR/appmanifest/#id-member, and
  // strips any #fragments from the url.
  // This constructor enforces validity via a hard CHECK.
  // The Create() function is the preferred way of ValidManifestId creation.
  explicit ValidManifestId(GURL url);

  ValidManifestId(const ValidManifestId& other);
  ValidManifestId& operator=(const ValidManifestId& other);
  ~ValidManifestId();

  bool is_valid() const;

  const std::string& spec() const;

  // Return underlying valid GURL without #fragments.
  const GURL& value() const;

  friend std::strong_ordering operator<=>(const ValidManifestId& lhs,
                                          const ValidManifestId& rhs);

  bool operator==(const ValidManifestId& other) const;

  bool operator==(const GURL& other) const;

  template <typename H>
  friend H AbslHashValue(H h, const ValidManifestId& manifest_id) {
    return H::combine(std::move(h), manifest_id.url_);
  }

 private:
  GURL url_;
};

std::ostream& operator<<(std::ostream& out, const ValidManifestId& manifest_id);

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_COMMON_MANIFEST_ID_H_
