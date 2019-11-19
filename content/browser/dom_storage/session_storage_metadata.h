// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CONTENT_BROWSER_DOM_STORAGE_SESSION_STORAGE_METADATA_H_
#define CONTENT_BROWSER_DOM_STORAGE_SESSION_STORAGE_METADATA_H_

#include <stdint.h>
#include <map>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "components/services/storage/dom_storage/async_dom_storage_database.h"
#include "components/services/storage/dom_storage/dom_storage_database.h"
#include "content/common/content_export.h"
#include "url/origin.h"

namespace content {

// Holds the metadata information for a session storage database. This includes
// logic for parsing and saving database content.
class CONTENT_EXPORT SessionStorageMetadata {
 public:
  // Version 0 represents the old SessionStorageDatabase where we never stored a
  // version. This class stores '0' as the version in this case. Version 1
  // removes 'namespaces-' dummy entry, and 'map-_-' refcount entries that are
  // present in version 0.
  static constexpr const int64_t kMinSessionStorageSchemaVersion = 0;
  static constexpr const int64_t kLatestSessionStorageSchemaVersion = 1;

  static constexpr const int64_t kInvalidDatabaseVersion = -1;
  static constexpr const int64_t kInvalidMapId = -1;

  static constexpr const uint8_t kDatabaseVersionBytes[] = {'v', 'e', 'r', 's',
                                                            'i', 'o', 'n'};

  static constexpr const uint8_t kNamespacePrefixBytes[] = {
      'n', 'a', 'm', 'e', 's', 'p', 'a', 'c', 'e', '-'};

  // This is "next-map-id" (without the quotes).
  static constexpr const uint8_t kNextMapIdKeyBytes[] = {
      'n', 'e', 'x', 't', '-', 'm', 'a', 'p', '-', 'i', 'd'};

  // Represents a map which can be shared by multiple areas.
  // The |DeleteNamespace| and |DeleteArea| methods can destroy any MapData
  // objects who are no longer referenced by another namespace.
  // Maps (and thus MapData objects) can only be shared for the same origin.
  class CONTENT_EXPORT MapData : public base::RefCounted<MapData> {
   public:
    explicit MapData(int64_t map_number, url::Origin origin);

    const url::Origin& origin() const { return origin_; }

    // The number of namespaces that reference this map.
    int ReferenceCount() const { return reference_count_; }

    // The key prefix for the map data (e.g. "map-2-").
    const std::vector<uint8_t>& KeyPrefix() const { return key_prefix_; }

    // The number of the map as bytes (e.g. "2").
    const std::vector<uint8_t>& MapNumberAsBytes() const {
      return number_as_bytes_;
    }

   private:
    friend class base::RefCounted<MapData>;
    friend class SessionStorageMetadata;
    ~MapData();

    void IncReferenceCount() { ++reference_count_; }
    void DecReferenceCount() { --reference_count_; }

    // The map number as bytes (e.g. "2"). These bytes are the string
    // representation of the map number.
    std::vector<uint8_t> number_as_bytes_;
    std::vector<uint8_t> key_prefix_;
    url::Origin origin_;
    int reference_count_ = 0;
  };

  using NamespaceOriginMap =
      std::map<std::string, std::map<url::Origin, scoped_refptr<MapData>>>;
  using NamespaceEntry = NamespaceOriginMap::iterator;

  SessionStorageMetadata();
  ~SessionStorageMetadata();

  // For a new database, this saves the database version, clears the metadata,
  // and returns the operations needed to save to disk.
  std::vector<storage::AsyncDomStorageDatabase::BatchDatabaseTask>
  SetupNewDatabase();

  // This parses the database version from the bytes that were stored on
  // disk, or if there was no version saved then passes a base::nullopt. This
  // call is not necessary on new databases. The |upgrade_tasks| are populated
  // with any tasks needed to upgrade the databases versioning metadata. Note
  // this is different than the namespaces metadata, which will be upgraded in
  // ParseNamespaces.
  //
  // Returns |true| if the parsing is correct and we support the version read.
  bool ParseDatabaseVersion(
      base::Optional<std::vector<uint8_t>> value,
      std::vector<storage::AsyncDomStorageDatabase::BatchDatabaseTask>*
          upgrade_tasks);

  // Parses all namespaces and maps, and stores all metadata locally. This
  // invalidates all NamespaceEntry and MapData objects. If there is a parsing
  // error, the namespaces will be cleared.If the version given to
  // |ParseDatabaseVersion| is an older version, any namespace metadata upgrades
  // will be populated in |upgrade_tasks|. This call is not necessary on new
  // databases.
  bool ParseNamespaces(
      std::vector<storage::DomStorageDatabase::KeyValuePair> values,
      std::vector<storage::AsyncDomStorageDatabase::BatchDatabaseTask>*
          upgrade_tasks);

  // Parses the next map id from the given bytes. If that fails, then it uses
  // the next available id from parsing the namespaces. This call is not
  // necessary on new databases.
  void ParseNextMapId(const std::vector<uint8_t>& map_id);

  // Creates new map data for the given namespace-origin area. If the area
  // entry exists, then it will decrement the refcount of the old map. Tasks
  // appended to |*save_tasks| if run will save the new or modified area entry
  // to disk, as well as saving the next available map id.
  //
  // NOTE: It is invalid to call this method for an area that has a map with
  // only one reference.
  scoped_refptr<MapData> RegisterNewMap(
      NamespaceEntry namespace_entry,
      const url::Origin& origin,
      std::vector<storage::AsyncDomStorageDatabase::BatchDatabaseTask>*
          save_tasks);

  // Registers an origin-map in the |destination_namespace| from every
  // origin-map in the |source_namespace|. The |destination_namespace| must have
  // no origin-maps. All maps in the destination namespace are the same maps as
  // the source namespace. All database operations to save the namespace origin
  // metadata are put in |save_tasks|.
  void RegisterShallowClonedNamespace(
      NamespaceEntry source_namespace,
      NamespaceEntry destination_namespace,
      std::vector<storage::AsyncDomStorageDatabase::BatchDatabaseTask>*
          save_tasks);

  // Deletes the given namespace and any maps that no longer have any
  // references. This will invalidate all NamespaceEntry objects for the
  // |namespace_id|, and can invalidate any MapData objects whose reference
  // count hits zero. Appends operations to |*save_tasks| which will commit the
  // deletions to disk if run.
  void DeleteNamespace(
      const std::string& namespace_id,
      std::vector<storage::AsyncDomStorageDatabase::BatchDatabaseTask>*
          save_tasks);

  // This returns a BatchDatabaseTask to remove the metadata entry for this
  // namespace-origin area. If the map at this entry isn't referenced by any
  // other area (refcount hits 0), then the task will also delete that map on
  // disk and invalidate that MapData.
  void DeleteArea(
      const std::string& namespace_id,
      const url::Origin& origin,
      std::vector<storage::AsyncDomStorageDatabase::BatchDatabaseTask>*
          save_tasks);

  NamespaceEntry GetOrCreateNamespaceEntry(const std::string& namespace_id);

  const NamespaceOriginMap& namespace_origin_map() const {
    return namespace_origin_map_;
  }

  int64_t NextMapId() const { return next_map_id_; }

 private:
  static std::vector<uint8_t> LatestDatabaseVersionAsVector();

  static std::vector<uint8_t> GetNamespacePrefix(
      const std::string& namespace_id);
  static std::vector<uint8_t> GetAreaKey(const std::string& namespace_id,
                                         const url::Origin& origin);
  static std::vector<uint8_t> GetMapPrefix(int64_t map_number);
  static std::vector<uint8_t> GetMapPrefix(
      const std::vector<uint8_t>& map_number_as_bytes);

  int64_t initial_database_version_from_disk_ = kInvalidDatabaseVersion;
  int64_t next_map_id_ = kInvalidMapId;
  int64_t next_map_id_from_namespaces_ = 0;

  NamespaceOriginMap namespace_origin_map_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOM_STORAGE_SESSION_STORAGE_METADATA_H_
