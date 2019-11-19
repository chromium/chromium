# SQLite Data Recovery

This directory implements data recovery heuristics for SQLite databases whose
files were corrupted on disk. The recovery code walks through the B-tree
holding a SQLite table and recovers all records that seem healthy. Even if
recovery succeeds, a recovered table may be missing records, and existing
records may have corrupted values inside them. Any constraints imposed by Chrome
or by SQLite may be broken.


## Usage

The default approach for handling corruption in SQLite databases is to
immediately stop using the database, delete it, and start over with a new
database. The recovery method implemented here is intended for databases used by
Chrome features that handle high-value user data, such as History and Bookmarks.
These features carefully handle data inconsistency edge cases, and their
database schemas are resilient to partial data loss.

The code is plugged into the rest of Chrome via
[SQLite's virtual table module API](https://sqlite.org/vtab.html). The example
below covers a typical recovery scenario.

```sql
-- Feature table schema.
CREATE TABLE data(name TEXT PRIMARY KEY, value TEXT NOT NULL);

-- Recover in another database. The corrupted one is unreliable.
ATTACH DATABASE '/tmp/db.db' as recovery;
-- Re-create the feature table's schema.
CREATE TABLE recovery.feature(name TEXT PRIMARY KEY, value TEXT NOT NULL);
-- Start reading the corrupted data.
CREATE VIRTUAL TABLE temp.recover_feature USING recover(
    main.feature, -- The corrupted database.
    -- Recovery will skip row values that don't have the TEXT type.
    name TEXT STRICT NOT NULL,
    -- Recovery will include any row value coercible to TEXT.
    value TEXT NOT NULL);
-- Data recovered from corrupted databases may not meet schema constraints, so
-- recovery insertions must use "OR REPLACE"  or "OR IGNORE".
INSERT OR REPLACE INTO recovery.feature(rowid, name, value)
SELECT rowid, name, value FROM temp.recover_feature;
-- Cleanup after the recovery operation.
DROP TABLE temp.recover_feature;
DETACH DATABASE recovery;
-- Replace the corrupted database file with the recovered one.
```

The feature invoking the recovery virtual table must know the schema of the
database being recovered. A generic high-level recovery layer should first
recover
[the `sqlite_master` table](https://www.sqlite.org/fileformat.html#storage_of_the_sql_database_schema),
which has a well known format, then use its contents to recover the schema of
any other table. This recovery module already relies on the integrity of the
`sqlite_master` table.

The column definitions in the virtual table creation statement must follow
the syntax _column\_name_ _type\_name_ [`STRICT`] [`NOT NULL`]. _type\_name_ is
[the SQLite data types](https://www.sqlite.org/datatype3.html#storage_classes_and_datatypes),
or the special types `ANY` or `ROWID`.

The `ANY` type can be used to recover values of all types.

The `ROWID` type must be used for columns that alias
[rowid](https://www.sqlite.org/lang_createtable.html#rowid). This typically only
happens when a column is declared as `INTEGER PRIMARY KEY`.
Designating `ROWID` columns is essential for decoding records correctly.

TODO(pwnall): Look into removing `STRICT`, if it's not used.


## Limitations

The current implementation only handles [table
B-trees](https://www.sqlite.org/fileformat.html#b_tree_pages). It cannot
recover [WITHOUT ROWID](https://www.sqlite.org/rowidtable.html) tables, which
are stored in index B-trees.


## Code Map

The code is structured as follows.

* integers.{cc,h} decodes the integer formats used by SQLite.
* btree.{cc,h} decodes the cells in SQLite's B-tree pages.
* payload.{cc,h} reads record payloads from B-tree pages and overflow pages.
* record.{cc,h} decodes column values from record.
* cursor.{cc,h} implements a SQLite virtual table cursor.
* table.{cc,h} implements one recovery virtual table.
* parsing.{cc,h} parses the SQL strings passed in via `CREATE VIRTUAL TABLE`
  and implements the constraints explained above.
* module.{cc,h} implements the SQLite virtual table interface.

The feature is tested by integration tests that issue SQLite queries.
