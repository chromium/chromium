// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_MDNS_CACHE_H_
#define NET_DNS_MDNS_CACHE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "net/base/net_export.h"

namespace net {

class RecordParsed;

// mDNS Cache
// This is a cache of mDNS records. It keeps track of expiration times and is
// guaranteed not to return expired records. It also has facilities for timely
// record expiration.
class NET_EXPORT_PRIVATE MDnsCache {
 public:
  // Key type for the record map. It is a 3-tuple of type, name and optional
  // value ordered by type, then name, then optional value. This allows us to
  // query for all records of a certain type and name, while also allowing us
  // to set records of a certain type, name and optionally value as unique.
  class Key {
   public:
    Key(unsigned type, const std::string& name, const std::string& optional);
    Key(const Key&);
    Key& operator=(const Key&);
    ~Key();
    bool operator<(const Key& key) const;
    bool operator==(const Key& key) const;

    unsigned type() const { return type_; }
    const std::string& name_lowercase() const { return name_lowercase_; }
    const std::string& optional() const { return optional_; }

    // Create the cache key corresponding to |record|.
    static Key CreateFor(const RecordParsed* record);
   private:
    unsigned type_;
    std::string name_lowercase_;
    std::string optional_;
  };

  typedef base::RepeatingCallback<void(const RecordParsed*)>
      RecordRemovedCallback;

  enum UpdateType {
    RecordAdded,
    RecordChanged,
    RecordRemoved,
    NoChange
  };

  MDnsCache();

  MDnsCache(const MDnsCache&) = delete;
  MDnsCache& operator=(const MDnsCache&) = delete;

  ~MDnsCache();

  // Return value indicates whether the record was added, changed
  // (existed previously with different value) or not changed (existed
  // previously with same value).
  UpdateType UpdateDnsRecord(std::unique_ptr<const RecordParsed> record);

  // Check cache for record with key |key|. Return the record if it exists, or
  // NULL if it doesn't.
  const RecordParsed* LookupKey(const Key& key);

  // Return records with type |type| and name |name|. Expired records will not
  // be returned. If |type| is zero, return all records with name |name|.
  void FindDnsRecords(unsigned type,
                      const std::string& name,
                      std::vector<const RecordParsed*>* records,
                      base::Time now) const;

  // Remove expired records, call |record_removed_callback| for every removed
  // record.
  void CleanupRecords(base::Time now,
                      const RecordRemovedCallback& record_removed_callback);

  // Returns a time less than or equal to the next time a record will expire.
  // Is updated when CleanupRecords or UpdateDnsRecord are called. Returns
  // base::Time when the cache is empty.
  base::Time next_expiration() const { return next_expiration_; }

  // Remove a record from the cache.  Returns a scoped version of the pointer
  // passed in if it was removed, scoped null otherwise.
  std::unique_ptr<const RecordParsed> RemoveRecord(const RecordParsed* record);

  bool IsCacheOverfilled() const;

  void set_entry_limit_for_testing(size_t entry_limit) {
    entry_limit_ = entry_limit;
  }

 private:
  typedef std::map<Key, std::unique_ptr<const RecordParsed>> RecordMap;

  // Get the effective expiration of a cache entry, based on its creation time
  // and TTL. Does adjustments so entries with a TTL of zero will have a
  // nonzero TTL, as explained in RFC 6762 Section 10.1.
  static base::Time GetEffectiveExpiration(const RecordParsed* entry);

  // Get optional part of the DNS key for shared records. For example, in PTR
  // records this is the pointed domain, since multiple PTR records may exist
  // for the same name.
  static std::string GetOptionalFieldForRecord(const RecordParsed* record);

  RecordMap mdns_cache_;

  base::Time next_expiration_;
  size_t entry_limit_;
};

}  // namespace net

#endif  // NET_DNS_MDNS_CACHE_H_
