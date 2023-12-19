// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_PERMISSIONS_API_PERMISSION_SET_H_
#define EXTENSIONS_COMMON_PERMISSIONS_API_PERMISSION_SET_H_

#include <stddef.h>

#include <set>
#include <string>
#include <vector>

#include "base/values.h"
#include "extensions/common/mojom/api_permission_id.mojom-shared.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/base_set_operators.h"

namespace extensions {

class APIPermissionSet;

template<>
struct BaseSetOperatorsTraits<APIPermissionSet> {
  using ElementType = APIPermission;
  using ElementIDType = mojom::APIPermissionID;
};

class APIPermissionSet : public BaseSetOperators<APIPermissionSet> {
 public:
  enum ParseSource {
    // Don't allow internal permissions to be parsed (e.g. entries in the
    // "permissions" list in a manifest).
    kDisallowInternalPermissions,

    // Allow internal permissions to be parsed (e.g. from the "api" field of a
    // permissions list in the prefs).
    kAllowInternalPermissions,
  };

  void insert(mojom::APIPermissionID id);

  // Inserts |permission| into the APIPermissionSet.
  void insert(std::unique_ptr<APIPermission> permission);

  // Parses permissions from |permissions| and adds the parsed permissions to
  // |api_permissions|. If |source| is kDisallowInternalPermissions, treat
  // permissions with kFlagInternal as errors. If |unhandled_permissions| is
  // not NULL, the names of all permissions that couldn't be parsed will be
  // added to this vector. If |error| is NULL, parsing will continue with the
  // next permission if invalid data is detected. If |error| is not NULL, it
  // will be set to an error message and false is returned when an invalid
  // permission is found.
  static bool ParseFromJSON(const base::Value::List& permissions,
                            ParseSource source,
                            APIPermissionSet* api_permissions,
                            std::u16string* error,
                            std::vector<std::string>* unhandled_permissions);
};

// An ID representing a single permission that belongs to an app or extension.
//
// Each PermissionID has a required ID to identify the permission. For most
// permissions, this is all they have.
//
// Some more complex permissions have a parameter, which acts like an argument
// for the permission. For example, host permissions might have the ID
// kReadOnlyHost and the argument 'www.google.com' (the host which is
// read-only). Parameters are passed to the permission message rules for this
// permission, so they can affect the displayed message.
//
// Note: Inheriting from std::pair automatically gives us an operator<
// (required for putting these into an std::set).
//
// TODO(sashab): Move this to the same file as PermissionIDSet once that moves
// to its own file.
class PermissionID : public std::pair<mojom::APIPermissionID, std::u16string> {
 public:
  explicit PermissionID(mojom::APIPermissionID id);
  PermissionID(mojom::APIPermissionID id, const std::u16string& parameter);
  virtual ~PermissionID();

  const mojom::APIPermissionID& id() const { return this->first; }
  const std::u16string& parameter() const { return this->second; }
};

// A set of permissions for an app or extension. Used for passing around groups
// of permissions, such as required or optional permissions.
//
// Each permission can also store a string, such as a hostname or device number,
// as a parameter that helps identify the permission. This parameter can then
// be used when the permission message is generated. For example, the permission
// kHostReadOnly might have the parameter "google.com", which means that the app
// or extension has the permission to read the host google.com. This parameter
// may then be included in the permission message when it is generated later.
//
// Example:
//   // Create an empty PermissionIDSet.
//   PermissionIDSet p;
//   // Add a permission to the set.
//   p.insert(mojom::APIPermissionID::kNetworkState);
//   // Add a permission with a parameter to the set.
//   p.insert(mojom::APIPermissionID::kHostReadOnly,
//            u"http://www.google.com");
//
// TODO(sashab): Move this to its own file and rename it to PermissionSet after
// APIPermission is removed, the current PermissionSet is no longer used, and
// mojom::APIPermissionID is the only type of Permission ID.
class PermissionIDSet {
 public:
  using const_iterator = std::set<PermissionID>::const_iterator;

  PermissionIDSet();
  PermissionIDSet(std::initializer_list<mojom::APIPermissionID> permissions);
  PermissionIDSet(const PermissionIDSet& other);
  virtual ~PermissionIDSet();

  // Adds the given permission, and an optional parameter, to the set.
  void insert(mojom::APIPermissionID permission_id);
  void insert(mojom::APIPermissionID permission_id,
              const std::u16string& permission_parameter);
  void InsertAll(const PermissionIDSet& permission_set);

  // Erases all permissions with the given id.
  void erase(mojom::APIPermissionID permission_id);

  // Returns the parameters for all PermissionIDs in this set.
  std::vector<std::u16string> GetAllPermissionParameters() const;

  // Check if the set contains a permission with the given ID.
  bool ContainsID(PermissionID permission_id) const;
  bool ContainsID(mojom::APIPermissionID permission_id) const;

  // Check if the set contains permissions with all the given IDs.
  bool ContainsAllIDs(
      const std::set<mojom::APIPermissionID>& permission_ids) const;

  // Check if the set contains any permission with one of the given IDs.
  bool ContainsAnyID(
      const std::set<mojom::APIPermissionID>& permission_ids) const;
  bool ContainsAnyID(const PermissionIDSet& other) const;

  // Returns all the permissions in this set with the given ID.
  PermissionIDSet GetAllPermissionsWithID(
      mojom::APIPermissionID permission_id) const;

  // Returns all the permissions in this set with one of the given IDs.
  PermissionIDSet GetAllPermissionsWithIDs(
      const std::set<mojom::APIPermissionID>& permission_ids) const;

  // Convenience functions for common set operations.
  bool Includes(const PermissionIDSet& subset) const;
  bool Equals(const PermissionIDSet& set) const;
  static PermissionIDSet Difference(const PermissionIDSet& set_1,
                                    const PermissionIDSet& set_2);

  size_t size() const;
  bool empty() const;

  const_iterator begin() const { return permissions_.begin(); }
  const_iterator end() const { return permissions_.end(); }

 private:
  explicit PermissionIDSet(const std::set<PermissionID>& permissions);

  std::set<PermissionID> permissions_;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_PERMISSIONS_API_PERMISSION_SET_H_
