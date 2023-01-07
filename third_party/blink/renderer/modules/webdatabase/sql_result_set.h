/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBDATABASE_SQL_RESULT_SET_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBDATABASE_SQL_RESULT_SET_H_

#include "third_party/blink/renderer/modules/webdatabase/database_basic_types.h"
#include "third_party/blink/renderer/modules/webdatabase/sql_result_set_row_list.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class ExceptionState;

class SQLResultSet final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  SQLResultSet();

  void Trace(Visitor*) const override;

  SQLResultSetRowList* rows() const;

  int64_t insertId(ExceptionState&) const;
  int64_t rowsAffected() const;

  // For internal (non-JS) use
  void SetInsertId(int64_t);
  void SetRowsAffected(int64_t);
  bool IsValid() { return is_valid_; }

 private:
  Member<SQLResultSetRowList> rows_;
  int64_t insert_id_ = 0;
  int64_t rows_affected_ = 0;
  bool insert_id_set_ = false;
  bool is_valid_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBDATABASE_SQL_RESULT_SET_H_
