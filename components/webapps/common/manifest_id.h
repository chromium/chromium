// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_COMMON_MANIFEST_ID_H_
#define COMPONENTS_WEBAPPS_COMMON_MANIFEST_ID_H_

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
  // #fragments from the url.
  static std::optional<ValidManifestId> Create(std::string_view url_str);

  // Creates a manifest 'id' from the `url`. This follows the manifest `id`
  // parsing
  // algorithm at https://www.w3.org/TR/appmanifest/#id-member, and strips any
  // #fragments from the url.
  static std::optional<ValidManifestId> Create(const GURL& url);

  // Creates a manifest 'id' from the `url`. This follows the manifest `id`
  // parsing algorithm at https://www.w3.org/TR/appmanifest/#id-member, and
  // strips any #fragments from the url.
  explicit ValidManifestId(GURL url);

  ValidManifestId(const ValidManifestId& other);
  ValidManifestId& operator=(const ValidManifestId& other);
  ~ValidManifestId();

  bool is_valid() const;

  const std::string& spec() const;

  const GURL& value() const;

  friend auto operator<=>(const ValidManifestId& lhs,
                          const ValidManifestId& rhs);

  bool operator==(const ValidManifestId& other) const;

 private:
  GURL url_;
};

std::ostream& operator<<(std::ostream& out, const ValidManifestId& manifest_id);

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_COMMON_MANIFEST_ID_H_
