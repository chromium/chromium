/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBDATABASE_SQLITE_SQLITE_STATEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBDATABASE_SQLITE_SQLITE_STATEMENT_H_

#include "base/dcheck_is_on.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "third_party/blink/renderer/modules/webdatabase/sqlite/sqlite_database.h"

struct sqlite3_stmt;

namespace blink {

class SQLValue;

class SQLiteStatement {
  USING_FAST_MALLOC(SQLiteStatement);

 public:
  SQLiteStatement(SQLiteDatabase&, const String&);

  SQLiteStatement(const SQLiteStatement&) = delete;
  SQLiteStatement& operator=(const SQLiteStatement&) = delete;

  ~SQLiteStatement();

  int Prepare();
  int BindText(int index, const String&);
  int BindDouble(int index, double);
  int BindNull(int index);
  int BindValue(int index, const SQLValue&);
  unsigned BindParameterCount() const;

  int Step();
  int Finalize();

  int PrepareAndStep() {
    if (int error = Prepare())
      return error;
    return Step();
  }

  // prepares, steps, and finalizes the query.
  // returns true if all 3 steps succeed with step() returning SQLITE_DONE
  // returns false otherwise
  bool ExecuteCommand();

  // Returns -1 on last-step failing.  Otherwise, returns number of rows
  // returned in the last step()
  int ColumnCount();

  String GetColumnName(int col);
  SQLValue GetColumnValue(int col);
  String GetColumnText(int col);
  int GetColumnInt(int col);
  int64_t GetColumnInt64(int col);

 private:
  const raw_ref<SQLiteDatabase> database_;
  String query_;
  raw_ptr<sqlite3_stmt, DanglingUntriaged> statement_;
#if DCHECK_IS_ON()
  bool is_prepared_ = false;
#endif
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBDATABASE_SQLITE_SQLITE_STATEMENT_H_
