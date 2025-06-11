// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PERMISSION_OVERRIDES_H_
#define CONTENT_PUBLIC_BROWSER_PERMISSION_OVERRIDES_H_

#include <optional>
#include <variant>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/types/optional_ref.h"
#include "content/common/content_export.h"
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

  // Set permission override for |permission| at |origin| to |status|.
  // Null |origin| specifies global overrides.
  void Set(base::optional_ref<const url::Origin> origin,
           blink::PermissionType permission,
           const blink::mojom::PermissionStatus& status);

  // Get override for |origin| set for |permission|, if specified.
  std::optional<blink::mojom::PermissionStatus> Get(
      const url::Origin& origin,
      blink::PermissionType permission) const;

  // Sets status for |permissions| to GRANTED in |origin|, and DENIED
  // for all others.
  // Null |origin| grants permissions globally for context.
  void GrantPermissions(base::optional_ref<const url::Origin> origin,
                        const std::vector<blink::PermissionType>& permissions);

 private:
  // Represents a canonical key for permission overrides.
  // TODO(crbug.com/421149173): Update PermissionKey to also store both an
  // embedding and requesting site tuple.
  class PermissionKey {
   public:
    // Constructor for specific origin scopes and permission types.
    // Null |origin| means the key's scope is considered global for |type|.
    PermissionKey(base::optional_ref<const url::Origin> origin,
                  blink::PermissionType type);

    // Constructor for a global key specific to a permission type.
    // Delegates to the primary constructor, signaling a global scope with null
    // origin.
    explicit PermissionKey(blink::PermissionType);

    PermissionKey();
    ~PermissionKey();

    PermissionKey(const PermissionKey&);
    PermissionKey& operator=(const PermissionKey&);
    PermissionKey(PermissionKey&&);
    PermissionKey& operator=(PermissionKey&&);

    friend auto operator<=>(const PermissionKey&,
                            const PermissionKey&) = default;

   private:
    // Represents the global state within a PermissionKey.
    // All instances of GlobalKey are considered identical for comparison
    // purposes.
    struct GlobalKey {
      friend auto operator<=>(const GlobalKey&, const GlobalKey&) = default;
    };
    std::variant<GlobalKey, url::Origin> scope_;
    blink::PermissionType type_;
  };

  base::flat_map<PermissionKey, blink::mojom::PermissionStatus> overrides_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PERMISSION_OVERRIDES_H_
