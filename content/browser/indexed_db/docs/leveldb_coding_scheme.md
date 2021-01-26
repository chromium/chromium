# LevelDB Coding Scheme

LevelDB stores key/value pairs. Keys and values are strings of bytes,
normally of type `std::string`.

The keys in the backing store are variable-length tuples with
different types of fields, described here using the notation «a, b, c,
...». Each key in the backing store starts with a ternary prefix:
«database id, object store id, index id». For each, the id `0` is
always reserved for metadata; other ids may be reserved as well.

*** aside
The prefix makes sure that data for a specific database, object store,
and index are grouped together. The locality is important for
performance: common operations should only need a minimal number of
seek operations. For example, all the metadata for a database is
grouped together so that reading that metadata only requires one seek.
***

Each key type has a class - in [`square brackets`] below - which knows
how to encode, decode, and compare that key type.

The term "user key" refers to an Indexed DB key value specified by
user code as opposed to the internal keys as described here.

Integer literals used below (e.g. «0, 0, 0, 201, ...») are defined as
constants in `indexed_db_leveldb_coding.cc`

*** note
**Warning:** In order to be compatible with LevelDB's Bloom filter each
bit of the encoded key needs to used and "not ignored" by the
comparator.
***

*** note
**Warning:** As a custom comparator is used, some code to handle obsolete
data is still needed as the sort order must be maintained.
***

- - - -

## Types

### Primitive Types

Generic types which may appear as parts of keys or values are:

* **Byte** - what it says on the tin
* **Bool** - single byte, 0 = false, otherwise true
* **Int** - int64_t >= 0; 8 bytes in little-endian order
* **VarInt** - int64_t >= 0; variable-width, little-endian, 7 bits per
  byte with high bit set until last
* **String** - UTF-16BE (must be byte-swapped on x86/x64/ARM); length
  must be implicit
* **StringWithLength** - VarInt prefix with length in code units (i.e.
  bytes/2), followed by String
* **Binary** - VarInt prefix with length in bytes, followed by data
  bytes
* **Double** - IEEE754 64-bit (double), in *host endianness*

*** aside
There is a mix of network-endian, little-endian, and host-endian. In
particular, the String encoding requires byte-swapping. Sorry about
that.
***

### IDBKey (keys and values)

First byte is the type, followed by type-specific serialization:

* Null (no key): `0` (Byte) _Should not appear in data._
* Number: `3` (Byte), Double
* Date: `2` (Byte), Double
* String: `1` (Byte), StringWithLength
* Binary: `6` (Byte), Binary
* Array: `4` (Byte), count (VarInt), IDBKey...

### IDBKeyPath (values)

* Null: `0` (Byte), `0` (Byte), `0` (Byte)
* String: `0` (Byte), `0` (Byte), `1` (Byte), StringWithLength
* Array: `0` (Byte), `0` (Byte), `2` (Byte), count (VarInt), StringWithLength...

*** note
**Compatibility:**
If length is < 3 or the first two bytes are not `0`, `0` whole value
is decoded as a String.
***

### Blob Journal (value)

Blob journals are zero-or-more instances of the structure:

```
{
  database_id (VarInt),
  blob_number (VarInt)
}
```

There is no length prefix; just read until you run out of data.

If the blob_number is `DatabaseMetaDataKey::kAllBlobsNumber`, the whole
database should be deleted.

### ExternalObject (value)

A external object (a blob, a file, or a File System Access handle) is zero-or-more
instances of the structure:

```
{
  object_type (IndexedDBExternalObject::ObjectType as byte]),
  /*for Blobs and Files only*/ blob_number (VarInt),
  /*for Blobs and Files only*/ type (StringWithLength), // may be empty
  /*for Blobs and Files only*/ size (VarInt),
  /*for Files only*/ filename (StringWithLength)
  /*for Files only*/ lastModified (VarInt, in microseconds)
  /*for File System Access Handles only*/ token (BinaryWithLength)
}
```

There is no length prefix; just read until you run out of data.

- - - -

## Key Prefix
[`KeyPrefix`]

Each key is prefixed with «database id, object store id, index id»
with `0` reserved for metadata.

To save space, the prefix is not encoded with the usual types. The
first byte defines the lengths of the other fields:

* The top 3 bits are the length of the database id - 1 (in bytes)
* The next 3 bits are the length of the object store id - 1 (in bytes)
* The bottom 2 bits are the length of the index id - 1 (in bytes)

This is followed by:

* The database id in little-endian order (1 - 8 bytes)
* The object store id in little-endian order (1 - 8 bytes)
* The index id in little-endian order (1 - 4 bytes)

- - - -

## Global metadata

The prefix is «0, 0, 0», followed by a metadata type byte:

key                                 | value
------------------------------------|------
«0, 0, 0, 0»                        | backing store schema version (Int) [`SchemaVersionKey`]
«0, 0, 0, 1»                        | maximum allocated database (Int) [`MaxDatabaseIdKey`]
«0, 0, 0, 2»                        | data format version (Int) [`DataVersionKey`]
«0, 0, 0, 3»                        | recovery BlobJournal [`RecoveryBlobJournalKey`]
«0, 0, 0, 4»                        | active BlobJournal [`ActiveBlobJournalKey`]
«0, 0, 0, 5»                        | earliest sweep time (microseconds) (Int) [`EarliestSweepKey`]
«0, 0, 0, 100, database id (VarInt)» | Existence implies the database id is in the free list  [`DatabaseFreeListKey`] - _obsolete_
«0, 0, 0, 201, origin (StringWithLength), database name (StringWithLength)» | Database id (Int) [`DatabaseNameKey`]

*** aside
Free lists (#100) are no longer used. The ID space is assumed to be
sufficient.
***

The data format version encodes a `content::IndexedDBDataFormatVersion` object.
It includes a 32-bit version for the V8 serialization code in its most
significant bits, and a 32-bit version for the Blink serialization code in its
least significant 32 bits.

## Database metadata
[`DatabaseMetaDataKey`]

The prefix is «database id, 0, 0» followed by a metadata type Byte:

key                    | value
-----------------------|-------
«database id, 0, 0, 0» | origin name (String)
«database id, 0, 0, 1» | database name (String)
«database id, 0, 0, 2» | IDB string version data (String) - _obsolete_
«database id, 0, 0, 3» | maximum allocated object store id (Int)
«database id, 0, 0, 4» | IDB integer version (VarInt)
«database id, 0, 0, 5» | blob number generator current number (VarInt)

*** aside
Early versions of the Indexed DB spec used strings for versions
(#2) instead of monotonically increasing integers.
***


### More database metadata

The prefix is «database id, 0, 0» followed by a type Byte. The object
store and index id are VarInt, the names are StringWithLength.

key                                                   | value
------------------------------------------------------|-------
«database id, 0, 0, 150, object store id»             | existence implies the object store id is in the free list [`ObjectStoreFreeListKey`] - _obsolete_
«database id, 0, 0, 151, object store id, index id»   | existence implies the index id is in the free list [`IndexFreeListKey`] - _obsolete_
«database id, 0, 0, 200, object store name»           | object store id (Int) [`ObjectStoreNamesKey`] - _obsolete_
«database id, 0, 0, 201, object store id, index name» | index id (Int) [`IndexNamesKey`] - _obsolete_

*** aside
Free lists (#150, #151) are no longer used. The ID space is assumed to
be sufficient.

The name-to-id mappings (#200, #201) are written but no longer read;
instead the mapping is inferred from the object store and index
metadata. _These should probably be removed._
***


## Object store metadata
[`ObjectStoreMetaDataKey`]

The prefix is «database id, 0, 0», followed by a type Byte (`50`),
then the object store id (VarInt), then a metadata type Byte.

key                                         | value
--------------------------------------------|-------
«database id, 0, 0, 50, object store id, 0» | object store name (String)
«database id, 0, 0, 50, object store id, 1» | key path (IDBKeyPath)
«database id, 0, 0, 50, object store id, 2» | auto increment flag (Bool)
«database id, 0, 0, 50, object store id, 3» | is evictable (Bool) - _obsolete_
«database id, 0, 0, 50, object store id, 4» | last version number (Int)
«database id, 0, 0, 50, object store id, 5» | maximum allocated index id (Int)
«database id, 0, 0, 50, object store id, 6» | "has key path" flag (Bool) - _obsolete_
«database id, 0, 0, 50, object store id, 7» | key generator current number (Int)

The version field is used to weed out stale index data. Whenever new
object store data is inserted, it gets a new version number, and new
index data is written with this number. When the index is used for
look-ups, entries are validated against the "exists" entries, and
records with old version numbers are deleted when they are encountered
in `GetPrimaryKeyViaIndex`, `IndexCursorImpl::LoadCurrentRow` and
`IndexKeyCursorImpl::LoadCurrentRow`.

*** aside
Evictable stores (#3) were present in early iterations of the Indexed
DB specification.

The key path was originally just a string (#1) or null (identified by
flag, #6). To support null, string, or array the coding is now
identified by the leading bytes in #1 - see **IDBKeyPath**.
***

*** note
**Compatibility:**
If #6 is not present then a String key path can be assumed.
If #7 is not present, the key generator state is lazily initialized
using the maximum numeric key present in existing data.
***


## Index metadata
[`IndexMetaDataKey`]

The prefix is «database id, 0, 0», followed by a type Byte (100), then
the object store id (VarInt), then the index id (VarInt), then a
metadata type Byte.

key                                                    | value
-------------------------------------------------------|-------
«database id, 0, 0, 100, object store id, index id, 0» | index name (String)
«database id, 0, 0, 100, object store id, index id, 1» | unique flag (Bool)
«database id, 0, 0, 100, object store id, index id, 2» | key path (IDBKeyPath)
«database id, 0, 0, 100, object store id, index id, 3» | multi-entry flag (Bool)

*** note
**Compatibility:**
If #3 is not present, the multi-entry flag is unset.
***


## Object store data
[`ObjectStoreDataKey`]

The reserved index id `1` is used in the prefix. The prefix is
followed the encoded IDB primary key (IDBKey). The data has a
version prefix followed by the serialized script value.

key                                                  | value
-----------------------------------------------------|-------
«database id, object store id, 1, user key (IDBKey)» | version (VarInt), serialized script value


## "Exists" entry
[`ExistsEntryKey`]

The reserved index id `2` is used in the prefix. The prefix is
followed the encoded IDB primary key (IDBKey).

key                                                  | value
-----------------------------------------------------|-------
«database id, object store id, 2, user key (IDBKey)» | version (VarInt)


## External Object entry table
[`ExternalObjectKey`]

The reserved index id `3` is used in the prefix. The prefix is
followed the encoded IDB primary key.

key                                                  | value
-----------------------------------------------------|-------
«database id, object store id, 3, user key (IDBKey)» | ExternalObject


## Index data
[`IndexDataKey`]

The prefix is followed by a type byte, the encoded index key (IDBKey),
a sequence number (VarInt), and the encoded primary key (IDBKey).

key | value
----|-------
«database id, object store id, index id, index key (IDBKey), sequence number (VarInt), primary key (IDBKey)» | version (VarInt), primary key (IDBKey)

The sequence number is obsolete; it was used to allow two entries with
the same user (index) key in non-unique indexes prior to the inclusion
of the primary key in the key itself. `0` is always written now
(which, as a VarInt, takes a single byte)

*** note
**Compatibility:**
The sequence number and primary key, or just the primary key may not
be present. In that case, enumerators that need the primary key must
access the value.
***
