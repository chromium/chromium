// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_LEVELDB_CODING_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_LEVELDB_CODING_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/strings/string_piece.h"
#include "components/services/storage/indexed_db/locks/partitioned_lock_id.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key_path.h"

namespace content {

namespace indexed_db {
// 0 - Initial version.
// 1 - Adds UserIntVersion to DatabaseMetaData.
// 2 - Adds DataVersion to to global metadata.
// 3 - Adds metadata needed for blob support.
// 4 - Adds size & last_modified to 'file' blob_info encodings.
// 5 - One time verification that blob files exist on disk.
const constexpr int64_t kLatestKnownSchemaVersion = 5;
}  // namespace indexed_db

CONTENT_EXPORT extern const unsigned char kMinimumIndexId;

CONTENT_EXPORT std::string MaxIDBKey();
CONTENT_EXPORT std::string MinIDBKey();

// DatabaseId, BlobNumber
typedef std::pair<int64_t, int64_t> BlobJournalEntryType;
typedef std::vector<BlobJournalEntryType> BlobJournalType;

CONTENT_EXPORT void EncodeByte(unsigned char value, std::string* into);
CONTENT_EXPORT void EncodeBool(bool value, std::string* into);

// Unlike EncodeVarInt, this is a 'dumb' implementation of a variable int
// encoder. It writes, little-endian', until there are no more '1' bits in the
// number. The Decoder must know how to calculate the size of the encoded int,
// typically by having this reside at the end of the value or key.
CONTENT_EXPORT void EncodeInt(int64_t value, std::string* into);
CONTENT_EXPORT void EncodeString(const std::u16string& value,
                                 std::string* into);
CONTENT_EXPORT void EncodeStringWithLength(const std::u16string& value,
                                           std::string* into);
CONTENT_EXPORT void EncodeBinary(const std::string& value, std::string* into);
CONTENT_EXPORT void EncodeBinary(base::span<const uint8_t> value,
                                 std::string* into);
CONTENT_EXPORT void EncodeDouble(double value, std::string* into);
CONTENT_EXPORT void EncodeIDBKey(const blink::IndexedDBKey& value,
                                 std::string* into);
// This function creates a byte stream that can be directly compared to other
// byte streams on a byte-by-byte basis and retain semantic ordering. This
// enables the value to be stored as a SQLite blob without a specialized
// collation operation. Unlike `EncodeIDBKey`, which makes use of length bytes,
// this operation re-encodes variable-length values in a way that supports
// sentinels.
CONTENT_EXPORT void EncodeSortableIDBKey(const blink::IndexedDBKey& value,
                                         std::string* into);
CONTENT_EXPORT void EncodeIDBKeyPath(const blink::IndexedDBKeyPath& value,
                                     std::string* into);
CONTENT_EXPORT void EncodeBlobJournal(const BlobJournalType& journal,
                                      std::string* into);

[[nodiscard]] CONTENT_EXPORT bool DecodeByte(base::StringPiece* slice,
                                             unsigned char* value);
[[nodiscard]] CONTENT_EXPORT bool DecodeBool(base::StringPiece* slice,
                                             bool* value);
[[nodiscard]] CONTENT_EXPORT bool DecodeInt(base::StringPiece* slice,
                                            int64_t* value);
[[nodiscard]] CONTENT_EXPORT bool DecodeString(base::StringPiece* slice,
                                               std::u16string* value);
[[nodiscard]] CONTENT_EXPORT bool DecodeStringWithLength(
    base::StringPiece* slice,
    std::u16string* value);
[[nodiscard]] CONTENT_EXPORT bool DecodeBinary(base::StringPiece* slice,
                                               std::string* value);
// The returned span is only valid as long as the date behind |slice| is
// still valid.
[[nodiscard]] CONTENT_EXPORT bool DecodeBinary(
    base::StringPiece* slice,
    base::span<const uint8_t>* value);
[[nodiscard]] CONTENT_EXPORT bool DecodeDouble(base::StringPiece* slice,
                                               double* value);
[[nodiscard]] CONTENT_EXPORT bool DecodeIDBKey(
    base::StringPiece* slice,
    std::unique_ptr<blink::IndexedDBKey>* value);
[[nodiscard]] CONTENT_EXPORT bool DecodeIDBKeyPath(
    base::StringPiece* slice,
    blink::IndexedDBKeyPath* value);
[[nodiscard]] CONTENT_EXPORT bool DecodeBlobJournal(base::StringPiece* slice,
                                                    BlobJournalType* journal);

CONTENT_EXPORT int CompareEncodedStringsWithLength(base::StringPiece* slice1,
                                                   base::StringPiece* slice2,
                                                   bool* ok);

[[nodiscard]] CONTENT_EXPORT bool ExtractEncodedIDBKey(base::StringPiece* slice,
                                                       std::string* result);

CONTENT_EXPORT int CompareEncodedIDBKeys(base::StringPiece* slice1,
                                         base::StringPiece* slice2,
                                         bool* ok);

CONTENT_EXPORT int Compare(const base::StringPiece& a,
                           const base::StringPiece& b,
                           bool index_keys);

CONTENT_EXPORT int CompareKeys(const base::StringPiece& a,
                               const base::StringPiece& b);

CONTENT_EXPORT int CompareIndexKeys(const base::StringPiece& a,
                                    const base::StringPiece& b);

// Logging support.
std::string IndexedDBKeyToDebugString(base::StringPiece key);

// TODO(estade): these lock id factories have nothing to do with level db
// coding and don't belong in this file.

// We can't use the database ID for the database lock because we need to hold
// this lock before we start reading/writing the database metadata, at which
// point we don't yet know the ID, but do know the name (which is unique
// anyway).
CONTENT_EXPORT PartitionedLockId
GetDatabaseLockId(std::u16string database_name);
CONTENT_EXPORT PartitionedLockId GetObjectStoreLockId(int64_t database_id,
                                                      int64_t object_store_id);

// TODO(dmurph): Modify all decoding methods to return something more sensible,
// as it is not obvious that they modify the input slice to remove the decoded
// bit. https://crbug.com/922225
class KeyPrefix {
 public:
  // These are serialized to disk; any new items must be appended, and none can
  // be deleted.
  enum Type {
    GLOBAL_METADATA = 0,
    DATABASE_METADATA = 1,
    OBJECT_STORE_DATA = 2,
    EXISTS_ENTRY = 3,
    INDEX_DATA = 4,
    INVALID_TYPE = 5,
    BLOB_ENTRY = 6
  };

  static const size_t kMaxDatabaseIdSizeBits = 3;
  static const size_t kMaxObjectStoreIdSizeBits = 3;
  static const size_t kMaxIndexIdSizeBits = 2;

  static const size_t kMaxDatabaseIdSizeBytes = 1ULL
                                                << kMaxDatabaseIdSizeBits;  // 8
  static const size_t kMaxObjectStoreIdSizeBytes =
      1ULL << kMaxObjectStoreIdSizeBits;                                   // 8
  static const size_t kMaxIndexIdSizeBytes = 1ULL << kMaxIndexIdSizeBits;  // 4

  static const size_t kMaxDatabaseIdBits =
      kMaxDatabaseIdSizeBytes * 8 - 1;  // 63
  static const size_t kMaxObjectStoreIdBits =
      kMaxObjectStoreIdSizeBytes * 8 - 1;                              // 63
  static const size_t kMaxIndexIdBits = kMaxIndexIdSizeBytes * 8 - 1;  // 31

  static const int64_t kMaxDatabaseId =
      (1ULL << kMaxDatabaseIdBits) - 1;  // max signed int64_t
  static const int64_t kMaxObjectStoreId =
      (1ULL << kMaxObjectStoreIdBits) - 1;  // max signed int64_t
  static const int64_t kMaxIndexId =
      (1ULL << kMaxIndexIdBits) - 1;  // max signed int32_t

  static const int64_t kInvalidId = -1;

  KeyPrefix();
  explicit KeyPrefix(int64_t database_id);
  KeyPrefix(int64_t database_id, int64_t object_store_id);
  KeyPrefix(int64_t database_id, int64_t object_store_id, int64_t index_id);
  static KeyPrefix CreateWithSpecialIndex(int64_t database_id,
                                          int64_t object_store_id,
                                          int64_t index_id);

  static bool Decode(base::StringPiece* slice, KeyPrefix* result);
  std::string Encode() const;
  static std::string EncodeEmpty();
  int Compare(const KeyPrefix& other) const;

  CONTENT_EXPORT static bool IsValidDatabaseId(int64_t database_id);
  static bool IsValidObjectStoreId(int64_t index_id);
  static bool IsValidIndexId(int64_t index_id);
  static bool ValidIds(int64_t database_id,
                       int64_t object_store_id,
                       int64_t index_id) {
    return IsValidDatabaseId(database_id) &&
           IsValidObjectStoreId(object_store_id) && IsValidIndexId(index_id);
  }
  static bool ValidIds(int64_t database_id, int64_t object_store_id) {
    return IsValidDatabaseId(database_id) &&
           IsValidObjectStoreId(object_store_id);
  }

  std::string DebugString();

  Type type() const;

  int64_t database_id_;
  int64_t object_store_id_;
  int64_t index_id_;

 private:
  // Special constructor for CreateWithSpecialIndex()
  KeyPrefix(enum Type,
            int64_t database_id,
            int64_t object_store_id,
            int64_t index_id);

  static std::string EncodeInternal(int64_t database_id,
                                    int64_t object_store_id,
                                    int64_t index_id);
};

class SchemaVersionKey {
 public:
  CONTENT_EXPORT static std::string Encode();
};

class MaxDatabaseIdKey {
 public:
  CONTENT_EXPORT static std::string Encode();
};

class DataVersionKey {
 public:
  CONTENT_EXPORT static std::string Encode();
};

class RecoveryBlobJournalKey {
 public:
  static std::string Encode();
};

class ActiveBlobJournalKey {
 public:
  static std::string Encode();
};

class EarliestSweepKey {
 public:
  static std::string Encode();
};

class EarliestCompactionKey {
 public:
  static std::string Encode();
};

class ScopesPrefix {
 public:
  CONTENT_EXPORT static std::vector<uint8_t> Encode();
};

class DatabaseFreeListKey {
 public:
  DatabaseFreeListKey();
  static bool Decode(base::StringPiece* slice, DatabaseFreeListKey* result);
  CONTENT_EXPORT static std::string Encode(int64_t database_id);
  static CONTENT_EXPORT std::string EncodeMaxKey();
  int64_t DatabaseId() const;
  int Compare(const DatabaseFreeListKey& other) const;
  std::string DebugString() const;

 private:
  int64_t database_id_;
};

class DatabaseNameKey {
 public:
  static bool Decode(base::StringPiece* slice, DatabaseNameKey* result);
  CONTENT_EXPORT static std::string Encode(const std::string& origin_identifier,
                                           const std::u16string& database_name);
  static std::string EncodeMinKeyForOrigin(
      const std::string& origin_identifier);
  static std::string EncodeStopKeyForOrigin(
      const std::string& origin_identifier);
  std::u16string origin() const { return origin_; }
  std::u16string database_name() const { return database_name_; }
  int Compare(const DatabaseNameKey& other);
  std::string DebugString() const;

 private:
  std::u16string origin_;  // TODO(jsbell): Store encoded strings, or just
                           // pointers.
  std::u16string database_name_;
};

class DatabaseMetaDataKey {
 public:
  enum MetaDataType {
    ORIGIN_NAME = 0,
    DATABASE_NAME = 1,
    USER_STRING_VERSION = 2,  // Obsolete
    MAX_OBJECT_STORE_ID = 3,
    USER_VERSION = 4,
    BLOB_KEY_GENERATOR_CURRENT_NUMBER = 5,
    MAX_SIMPLE_METADATA_TYPE = 6
  };

  CONTENT_EXPORT static const int64_t kAllBlobsNumber;
  static const int64_t kBlobNumberGeneratorInitialNumber;
  // All keys <= 0 are invalid.  This one's just a convenient example.
  static const int64_t kInvalidBlobNumber;

  CONTENT_EXPORT static bool IsValidBlobNumber(int64_t blob_number);
  CONTENT_EXPORT static std::string Encode(int64_t database_id,
                                           MetaDataType type);
};

class ObjectStoreMetaDataKey {
 public:
  enum MetaDataType {
    NAME = 0,
    KEY_PATH = 1,
    AUTO_INCREMENT = 2,
    EVICTABLE = 3,
    LAST_VERSION = 4,
    MAX_INDEX_ID = 5,
    HAS_KEY_PATH = 6,
    KEY_GENERATOR_CURRENT_NUMBER = 7
  };

  // From the IndexedDB specification.
  static const int64_t kKeyGeneratorInitialNumber;

  ObjectStoreMetaDataKey();
  static bool Decode(base::StringPiece* slice, ObjectStoreMetaDataKey* result);
  CONTENT_EXPORT static std::string Encode(int64_t database_id,
                                           int64_t object_store_id,
                                           unsigned char meta_data_type);
  CONTENT_EXPORT static std::string EncodeMaxKey(int64_t database_id);
  CONTENT_EXPORT static std::string EncodeMaxKey(int64_t database_id,
                                                 int64_t object_store_id);
  int64_t ObjectStoreId() const;
  unsigned char MetaDataType() const;
  int Compare(const ObjectStoreMetaDataKey& other);
  std::string DebugString() const;

 private:
  int64_t object_store_id_;
  unsigned char meta_data_type_;
};

class IndexMetaDataKey {
 public:
  enum MetaDataType { NAME = 0, UNIQUE = 1, KEY_PATH = 2, MULTI_ENTRY = 3 };

  IndexMetaDataKey();
  static bool Decode(base::StringPiece* slice, IndexMetaDataKey* result);
  CONTENT_EXPORT static std::string Encode(int64_t database_id,
                                           int64_t object_store_id,
                                           int64_t index_id,
                                           unsigned char meta_data_type);
  CONTENT_EXPORT static std::string EncodeMaxKey(int64_t database_id,
                                                 int64_t object_store_id);
  CONTENT_EXPORT static std::string EncodeMaxKey(int64_t database_id,
                                                 int64_t object_store_id,
                                                 int64_t index_id);
  int Compare(const IndexMetaDataKey& other);
  std::string DebugString() const;

  int64_t IndexId() const;
  unsigned char meta_data_type() const { return meta_data_type_; }

 private:
  int64_t object_store_id_;
  int64_t index_id_;
  unsigned char meta_data_type_;
};

class ObjectStoreFreeListKey {
 public:
  ObjectStoreFreeListKey();
  static bool Decode(base::StringPiece* slice, ObjectStoreFreeListKey* result);
  CONTENT_EXPORT static std::string Encode(int64_t database_id,
                                           int64_t object_store_id);
  CONTENT_EXPORT static std::string EncodeMaxKey(int64_t database_id);
  int64_t ObjectStoreId() const;
  int Compare(const ObjectStoreFreeListKey& other);
  std::string DebugString() const;

 private:
  int64_t object_store_id_;
};

class IndexFreeListKey {
 public:
  IndexFreeListKey();
  static bool Decode(base::StringPiece* slice, IndexFreeListKey* result);
  CONTENT_EXPORT static std::string Encode(int64_t database_id,
                                           int64_t object_store_id,
                                           int64_t index_id);
  CONTENT_EXPORT static std::string EncodeMaxKey(int64_t database_id,
                                                 int64_t object_store_id);
  int Compare(const IndexFreeListKey& other);
  int64_t ObjectStoreId() const;
  int64_t IndexId() const;
  std::string DebugString() const;

 private:
  int64_t object_store_id_;
  int64_t index_id_;
};

class ObjectStoreNamesKey {
 public:
  // TODO(jsbell): We never use this to look up object store ids,
  // because a mapping is kept in the IndexedDBDatabase. Can the
  // mapping become unreliable?  Can we remove this?
  static bool Decode(base::StringPiece* slice, ObjectStoreNamesKey* result);
  CONTENT_EXPORT static std::string Encode(
      int64_t database_id,
      const std::u16string& object_store_name);
  int Compare(const ObjectStoreNamesKey& other);
  std::string DebugString() const;

  std::u16string object_store_name() const { return object_store_name_; }

 private:
  // TODO(jsbell): Store the encoded string, or just pointers to it.
  std::u16string object_store_name_;
};

class IndexNamesKey {
 public:
  IndexNamesKey();
  // TODO(jsbell): We never use this to look up index ids, because a mapping
  // is kept at a higher level.
  static bool Decode(base::StringPiece* slice, IndexNamesKey* result);
  CONTENT_EXPORT static std::string Encode(int64_t database_id,
                                           int64_t object_store_id,
                                           const std::u16string& index_name);
  int Compare(const IndexNamesKey& other);
  std::string DebugString() const;

  std::u16string index_name() const { return index_name_; }

 private:
  int64_t object_store_id_;
  std::u16string index_name_;
};

class ObjectStoreDataKey {
 public:
  static const int64_t kSpecialIndexNumber;

  ObjectStoreDataKey();
  ~ObjectStoreDataKey();

  static bool Decode(base::StringPiece* slice, ObjectStoreDataKey* result);
  CONTENT_EXPORT static std::string Encode(int64_t database_id,
                                           int64_t object_store_id,
                                           const std::string encoded_user_key);
  CONTENT_EXPORT static std::string Encode(int64_t database_id,
                                           int64_t object_store_id,
                                           const blink::IndexedDBKey& user_key);
  std::string DebugString() const;

  std::unique_ptr<blink::IndexedDBKey> user_key() const;

 private:
  std::string encoded_user_key_;
};

class ExistsEntryKey {
 public:
  ExistsEntryKey();

  ExistsEntryKey(const ExistsEntryKey&) = delete;
  ExistsEntryKey& operator=(const ExistsEntryKey&) = delete;

  ~ExistsEntryKey();

  static bool Decode(base::StringPiece* slice, ExistsEntryKey* result);
  CONTENT_EXPORT static std::string Encode(int64_t database_id,
                                           int64_t object_store_id,
                                           const std::string& encoded_key);
  static std::string Encode(int64_t database_id,
                            int64_t object_store_id,
                            const blink::IndexedDBKey& user_key);
  std::string DebugString() const;

  std::unique_ptr<blink::IndexedDBKey> user_key() const;

 private:
  static const int64_t kSpecialIndexNumber;

  std::string encoded_user_key_;
};

class CONTENT_EXPORT BlobEntryKey {
 public:
  BlobEntryKey() : database_id_(0), object_store_id_(0) {}
  static bool Decode(base::StringPiece* slice, BlobEntryKey* result);
  static bool FromObjectStoreDataKey(base::StringPiece* slice,
                                     BlobEntryKey* result);
  static std::string ReencodeToObjectStoreDataKey(base::StringPiece* slice);
  static std::string EncodeMinKeyForObjectStore(int64_t database_id,
                                                int64_t object_store_id);
  static std::string EncodeStopKeyForObjectStore(int64_t database_id,
                                                 int64_t object_store_id);
  static std::string Encode(int64_t database_id,
                            int64_t object_store_id,
                            const blink::IndexedDBKey& user_key);
  std::string Encode() const;
  std::string DebugString() const;

  int64_t database_id() const { return database_id_; }
  int64_t object_store_id() const { return object_store_id_; }

 private:
  static const int64_t kSpecialIndexNumber;

  static std::string Encode(int64_t database_id,
                            int64_t object_store_id,
                            const std::string& encoded_user_key);
  int64_t database_id_;
  int64_t object_store_id_;
  // This is the user's ObjectStoreDataKey, not the BlobEntryKey itself.
  std::string encoded_user_key_;
};

class IndexDataKey {
 public:
  CONTENT_EXPORT IndexDataKey();
  CONTENT_EXPORT IndexDataKey(IndexDataKey&& other);

  IndexDataKey(const IndexDataKey&) = delete;
  IndexDataKey& operator=(const IndexDataKey&) = delete;

  CONTENT_EXPORT ~IndexDataKey();

  CONTENT_EXPORT static bool Decode(base::StringPiece* slice,
                                    IndexDataKey* result);
  CONTENT_EXPORT static std::string Encode(
      int64_t database_id,
      int64_t object_store_id,
      int64_t index_id,
      const std::string& encoded_user_key,
      const std::string& encoded_primary_key,
      int64_t sequence_number);
  static std::string Encode(int64_t database_id,
                            int64_t object_store_id,
                            int64_t index_id,
                            const blink::IndexedDBKey& user_key);
  CONTENT_EXPORT static std::string Encode(
      int64_t database_id,
      int64_t object_store_id,
      int64_t index_id,
      const blink::IndexedDBKey& user_key,
      const blink::IndexedDBKey& user_primary_key);
  CONTENT_EXPORT static std::string EncodeMinKey(int64_t database_id,
                                                 int64_t object_store_id,
                                                 int64_t index_id);

  // An index's keys are guaranteed to fall in [EncodeMinKey(), EncodeMaxKey()]
  CONTENT_EXPORT static std::string EncodeMaxKey(int64_t database_id,
                                                 int64_t object_store_id,
                                                 int64_t index_id);
  int64_t DatabaseId() const;
  int64_t ObjectStoreId() const;
  int64_t IndexId() const;
  std::unique_ptr<blink::IndexedDBKey> user_key() const;
  std::unique_ptr<blink::IndexedDBKey> primary_key() const;

  CONTENT_EXPORT std::string Encode() const;

  std::string DebugString() const;

 private:
  int64_t database_id_;
  int64_t object_store_id_;
  int64_t index_id_;
  std::string encoded_user_key_;
  std::string encoded_primary_key_;
  int64_t sequence_number_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_LEVELDB_CODING_H_
