// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INSTANCE_SQLITE_RECORD_ITERATOR_H_
#define CONTENT_BROWSER_INDEXED_DB_INSTANCE_SQLITE_RECORD_ITERATOR_H_

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "content/browser/indexed_db/status.h"

namespace blink {
class IndexedDBKey;
}  // namespace blink

namespace sql {
class Statement;
}

namespace content::indexed_db {
class Record;

namespace sqlite {

// Iterates over a non-empty range of object store/index records, maintaining
// the position in the range and emitting `Record`s.
class RecordIterator {
 public:
  using PositionAndRecord = std::pair<std::string, std::unique_ptr<Record>>;
  using BindCallback = base::RepeatingCallback<void(
      sql::Statement& statement,
      const std::string& position,
      const blink::IndexedDBKey& target_key,
      const blink::IndexedDBKey& target_primary_key,
      uint32_t offset)>;
  using ReadCallback = base::RepeatingCallback<StatusOr<PositionAndRecord>(
      sql::Statement& statement)>;

  RecordIterator(std::unique_ptr<sql::Statement> statement,
                 BindCallback bind_parameters,
                 ReadCallback read_row,
                 std::string initial_position);

  ~RecordIterator();

  // Iterates from the current position until the target `key` and `primary_key`
  // are reached. Use when at least one of these is valid.
  StatusOr<std::unique_ptr<Record>> Iterate(
      const blink::IndexedDBKey& key,
      const blink::IndexedDBKey& primary_key);

  // Iterates `count` times from the current position.
  StatusOr<std::unique_ptr<Record>> Iterate(uint32_t count);

 private:
  // The parsed and bound statement that embeds the SQL query for this iterator.
  // Because the records contained in the range can change between `Iterate()`
  // calls, the query needs to be typically re-executed every time. The query
  // itself is immutable for the duration of `this`, however, and contains a mix
  // of fixed and variable bound parameters. To avoid re-parsing the query and
  // rebinding the fixed parameters every time, hold on to the prepared
  // statement and rebind only the variable parameters as needed.
  std::unique_ptr<sql::Statement> statement_;

  // Callback to bind variable parameters to the statement.
  BindCallback bind_parameters_;

  // Callback to read the position and `Record` corresponding to the current row
  // of the statement.
  ReadCallback read_row_;

  // Opaque value tracking the position in the range. Typically, this is the
  // encoded key from the current record. Null when and only when `this` has
  // iterated past the end of its range.
  std::optional<std::string> current_position_;
};

}  // namespace sqlite
}  // namespace content::indexed_db

#endif  // CONTENT_BROWSER_INDEXED_DB_INSTANCE_SQLITE_RECORD_ITERATOR_H_
