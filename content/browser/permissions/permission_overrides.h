// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PERMISSIONS_PERMISSION_OVERRIDES_H_
#define CONTENT_BROWSER_PERMISSIONS_PERMISSION_OVERRIDES_H_

#include <optional>
#include <variant>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/types/optional_ref.h"
#include "components/content_settings/core/common/content_settings.h"
#include "content/common/content_export.h"
#include "content/public/browser/permission_result.h"
#include "net/base/schemeful_site.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"
#include "url/origin.h"

namespace blink {
enum class PermissionType;
}

namespace content {

// Maintains permission overrides for each origin.
class CONTENT_EXPORT PermissionOverrides {
 public:
  PermissionOverrides();
  ~PermissionOverrides();
  PermissionOverrides(PermissionOverrides&& other);
  PermissionOverrides& operator=(PermissionOverrides&& other);

  PermissionOverrides(const PermissionOverrides&) = delete;
  PermissionOverrides& operator=(const PermissionOverrides&) = delete;

  // Set permission override for |permission| at |requesting_origin| and
  // |embedding_origin| to |status|. Null |requesting_origin| and
  // |embedding_origin| specifies global overrides.
  // |requesting_origin| and |embedding_origin| must either both be null or both
  // be non-null.
  void Set(base::optional_ref<const url::Origin> requesting_origin,
           base::optional_ref<const url::Origin> embedding_origin,
           blink::PermissionType permission,
           const blink::mojom::PermissionStatus& status);

  // Get override for |requesting_origin| and |embedding_origin| set for
  // |permission|, if specified.
  std::optional<PermissionResult> Get(const url::Origin& requesting_origin,
                                      const url::Origin& embedding_origin,
                                      blink::PermissionType permission) const;

  // Creates content settings for all overrides for |permission_type|.
  std::vector<ContentSettingPatternSource> CreateContentSettingsForType(
      blink::PermissionType permission_type) const;

  // Sets status for |permissions| to GRANTED in |requesting_origin| and
  // |embedding_origin|, and DENIED for all others. Null |requesting_origin| and
  // |embedding_origin| grants permissions globally for context.
  // |requesting_origin| and |embedding_origin| must either both be null or both
  // be non-null.
  void GrantPermissions(base::optional_ref<const url::Origin> requesting_origin,
                        base::optional_ref<const url::Origin> embedding_origin,
                        const std::vector<blink::PermissionType>& permissions);

 private:
  // Represents a canonical key for permission overrides.
  class PermissionKey {
   public:
    PermissionKey(base::optional_ref<const url::Origin> requesting_origin,
                  base::optional_ref<const url::Origin> embedding_origin,
                  blink::PermissionType type);

    // Constructor for a global key specific to a permission type. Delegates to
    // the primary constructor, signaling a global scope.
    explicit PermissionKey(blink::PermissionType);

    PermissionKey();
    ~PermissionKey();

    PermissionKey(const PermissionKey&);
    PermissionKey& operator=(const PermissionKey&);
    PermissionKey(PermissionKey&&);
    PermissionKey& operator=(PermissionKey&&);

    friend auto operator<=>(const PermissionKey&,
                            const PermissionKey&) = default;

    // The pair is ordered as <Requesting-Origin, Embedding-Origin>
    std::pair<ContentSettingsPattern, ContentSettingsPattern>
    CreateContentSettingsPatterns() const;

    blink::PermissionType type() const { return type_; }

   private:
    // Represents the global state within a PermissionKey.
    // All instances of GlobalKey are considered identical for comparison
    // purposes.
    struct GlobalKey {
      friend auto operator<=>(const GlobalKey&, const GlobalKey&) = default;
    };
    using PermissionScope =
        std::variant<GlobalKey,
                     url::Origin,
                     std::pair<net::SchemefulSite, net::SchemefulSite>,
                     std::pair<url::Origin, net::SchemefulSite>>;

    static PermissionScope MakeScopeData(
        base::optional_ref<const url::Origin> requesting_origin,
        base::optional_ref<const url::Origin> embedding_origin,
        blink::PermissionType type);

    PermissionScope scope_;
    blink::PermissionType type_;
  };

  base::flat_map<PermissionKey, blink::mojom::PermissionStatus> overrides_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PERMISSIONS_PERMISSION_OVERRIDES_H_
