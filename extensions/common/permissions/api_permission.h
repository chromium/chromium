// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_PERMISSIONS_API_PERMISSION_H_
#define EXTENSIONS_COMMON_PERMISSIONS_API_PERMISSION_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "extensions/common/mojom/api_permission_id.mojom-shared.h"

namespace extensions {

class PermissionIDSet;
class APIPermissionInfo;
class PermissionsInfo;

// APIPermission is for handling some complex permissions. Please refer to
// extensions::SocketPermission as an example.
// There is one instance per permission per loaded extension.
class APIPermission {
 public:
  struct CheckParam {
  };

  explicit APIPermission(const APIPermissionInfo* info);

  virtual ~APIPermission();

  // Returns the id of this permission.
  mojom::APIPermissionID id() const;

  // Returns the name of this permission.
  const char* name() const;

  // Returns the APIPermission of this permission.
  const APIPermissionInfo* info() const {
    return info_;
  }

  // The set of permissions an app/extension with this API permission has. These
  // permissions are used by PermissionMessageProvider to generate meaningful
  // permission messages for the app/extension.
  //
  // For simple API permissions, this will return a set containing only the ID
  // of the permission. More complex permissions might have multiple IDs, one
  // for each of the capabilities the API permission has (e.g. read, write and
  // copy, in the case of the media gallery permission). Permissions that
  // require parameters may also contain a parameter string (along with the
  // permission's ID) which can be substituted into the permission message if a
  // rule is defined to do so.
  //
  // Permissions with multiple values, such as host permissions, are represented
  // by multiple entries in this set. Each permission in the subset has the same
  // ID (e.g. kHostReadOnly) but a different parameter (e.g. google.com). These
  // are grouped to form different kinds of permission messages (e.g. 'Access to
  // 2 hosts') depending on the number that are in the set. The rules that
  // define the grouping of related permissions with the same ID is defined in
  // ChromePermissionMessageProvider.
  virtual PermissionIDSet GetPermissions() const = 0;

  // Returns true if the given permission is allowed.
  virtual bool Check(const CheckParam* param) const = 0;

  // Returns true if |rhs| is a subset of this.
  virtual bool Contains(const APIPermission* rhs) const = 0;

  // Returns true if |rhs| is equal to this.
  virtual bool Equal(const APIPermission* rhs) const = 0;

  // Parses the APIPermission from |value|. Returns false if an error happens
  // and optionally set |error| if |error| is not NULL. If |value| represents
  // multiple permissions, some are invalid, and |unhandled_permissions| is
  // not NULL, the invalid ones are put into |unhandled_permissions| and the
  // function returns true.
  virtual bool FromValue(const base::Value* value,
                         std::string* error,
                         std::vector<std::string>* unhandled_permissions) = 0;

  // Stores this into a new created |value|.
  virtual std::unique_ptr<base::Value> ToValue() const = 0;

  // Clones this.
  virtual std::unique_ptr<APIPermission> Clone() const = 0;

  // Returns a new API permission which equals this - |rhs|.
  virtual std::unique_ptr<APIPermission> Diff(
      const APIPermission* rhs) const = 0;

  // Returns a new API permission which equals the union of this and |rhs|.
  virtual std::unique_ptr<APIPermission> Union(
      const APIPermission* rhs) const = 0;

  // Returns a new API permission which equals the intersect of this and |rhs|.
  virtual std::unique_ptr<APIPermission> Intersect(
      const APIPermission* rhs) const = 0;

 private:
  const raw_ptr<const APIPermissionInfo> info_;
};


// The APIPermissionInfo is an immutable class that describes a single
// named permission (API permission).
// There is one instance per permission.
class APIPermissionInfo {
 public:
  enum Flag {
    kFlagNone = 0,

    // Plugins (NPAPI) are deprecated.
    // kFlagImpliesFullAccess = 1 << 0,

    // Indicates if the permission implies full URL access.
    kFlagImpliesFullURLAccess = 1 << 1,

    // Indicates that extensions cannot specify the permission as optional.
    kFlagCannotBeOptional = 1 << 3,

    // Indicates that the permission is internal to the extensions
    // system and cannot be specified in the "permissions" list.
    kFlagInternal = 1 << 4,

    // Indicates that the permission may be granted to web contents by
    // extensions using the content_capabilities manifest feature.
    kFlagSupportsContentCapabilities = 1 << 5,

    // Indicates whether the permission should trigger one of the powerful
    // permissions messages in chrome://management. Reach out to the privacy
    // team when you add a new permission to check whether you should set this
    // flag or not.
    kFlagRequiresManagementUIWarning = 1 << 6,

    // Indicates that the permission shouldn't trigger the full warning on
    // the login screen of the managed-guest session. See
    // prefs::kManagedSessionUseFullLoginWarning. Most permissions are
    // considered powerful enough to warrant the full warning,
    // so the default for permissions (by not including this flag) is to trigger
    // it. Reach out to the privacy team when you add a new permission to check
    // whether you should set this flag or not.
    kFlagDoesNotRequireManagedSessionFullLoginWarning = 1 << 7
  };

  using APIPermissionConstructor =
      std::unique_ptr<APIPermission> (*)(const APIPermissionInfo*);

  using IDSet = std::set<mojom::APIPermissionID>;

  // This exists to allow aggregate initialization, so that default values
  // for flags, etc. can be omitted.
  // TODO(yoz): Simplify the way initialization is done. APIPermissionInfo
  // should be the simple data struct.
  struct InitInfo {
    mojom::APIPermissionID id;
    const char* name;
    int flags;
    APIPermissionInfo::APIPermissionConstructor constructor;
  };

  ~APIPermissionInfo();

  // Creates a APIPermission instance.
  std::unique_ptr<APIPermission> CreateAPIPermission() const;

  int flags() const { return flags_; }

  mojom::APIPermissionID id() const { return id_; }

  // Returns the name of this permission.
  const char* name() const { return name_; }

  // Returns true if this permission implies full URL access.
  bool implies_full_url_access() const {
    return (flags_ & kFlagImpliesFullURLAccess) != 0;
  }

  // Returns true if this permission can be added and removed via the
  // optional permissions extension API.
  bool supports_optional() const {
    return (flags_ & kFlagCannotBeOptional) == 0;
  }

  // Returns true if this permission is internal rather than a
  // "permissions" list entry.
  bool is_internal() const {
    return (flags_ & kFlagInternal) != 0;
  }

  // Returns true if this permission can be granted to web contents by an
  // extension through the content_capabilities manifest feature.
  bool supports_content_capabilities() const {
    return (flags_ & kFlagSupportsContentCapabilities) != 0;
  }

  // Returns true if this permission should trigger a warning on the management
  // page.
  bool requires_management_ui_warning() const {
    return (flags_ & kFlagRequiresManagementUIWarning) != 0;
  }

  // Returns true if this permission should trigger the full warning on the
  // login screen of the managed guest session.
  bool requires_managed_session_full_login_warning() const {
    return (flags_ & kFlagDoesNotRequireManagedSessionFullLoginWarning) == 0;
  }

 private:
  // Instances should only be constructed from within a PermissionsInfo.
  friend class PermissionsInfo;
  // Implementations of APIPermission will want to get the permission message,
  // but this class's implementation should be hidden from everyone else.
  friend class APIPermission;

  explicit APIPermissionInfo(const InitInfo& info);

  const char* const name_;
  const mojom::APIPermissionID id_;
  const int flags_;
  const APIPermissionConstructor api_permission_constructor_;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_PERMISSIONS_API_PERMISSION_H_
