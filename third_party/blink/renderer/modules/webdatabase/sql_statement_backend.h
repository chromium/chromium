/*
 * Copyright (C) 2007, 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBDATABASE_SQL_STATEMENT_BACKEND_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBDATABASE_SQL_STATEMENT_BACKEND_H_

#include <memory>
#include "third_party/blink/renderer/modules/webdatabase/sqlite/sql_value.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class Database;
class SQLErrorData;
class SQLResultSet;
class SQLStatement;

class SQLStatementBackend final : public GarbageCollected<SQLStatementBackend> {
 public:
  SQLStatementBackend(SQLStatement*,
                      const String& statement,
                      const Vector<SQLValue>& arguments,
                      int permissions);

  void Trace(Visitor*) const;

  bool Execute(Database*);
  bool LastExecutionFailedDueToQuota() const;

  bool HasStatementCallback() const { return has_callback_; }
  bool HasStatementErrorCallback() const { return has_error_callback_; }

  void SetVersionMismatchedError(Database*);

  SQLStatement* GetFrontend();
  SQLErrorData* SqlError() const;
  SQLResultSet* SqlResultSet() const;

 private:
  void SetFailureDueToQuota(Database*);
  void ClearFailureDueToQuota();

  Member<SQLStatement> frontend_;
  String statement_;
  Vector<SQLValue> arguments_;
  bool has_callback_;
  bool has_error_callback_;

  std::unique_ptr<SQLErrorData> error_;
  Member<SQLResultSet> result_set_;

  int permissions_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBDATABASE_SQL_STATEMENT_BACKEND_H_
