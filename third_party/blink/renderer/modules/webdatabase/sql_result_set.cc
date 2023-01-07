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

#include "third_party/blink/renderer/modules/webdatabase/sql_result_set.h"

#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

SQLResultSet::SQLResultSet()
    : rows_(MakeGarbageCollected<SQLResultSetRowList>()) {
  DCHECK(IsMainThread());
}

void SQLResultSet::Trace(Visitor* visitor) const {
  visitor->Trace(rows_);
  ScriptWrappable::Trace(visitor);
}

int64_t SQLResultSet::insertId(ExceptionState& exception_state) const {
  // 4.11.4 - Return the id of the last row inserted as a result of the query
  // If the query didn't result in any rows being added, raise an
  // InvalidAccessError exception.
  if (insert_id_set_)
    return insert_id_;

  exception_state.ThrowDOMException(
      DOMExceptionCode::kInvalidAccessError,
      "The query didn't result in any rows being added.");
  return -1;
}

int64_t SQLResultSet::rowsAffected() const {
  return rows_affected_;
}

SQLResultSetRowList* SQLResultSet::rows() const {
  return rows_.Get();
}

void SQLResultSet::SetInsertId(int64_t id) {
  DCHECK(!insert_id_set_);

  insert_id_ = id;
  insert_id_set_ = true;
}

void SQLResultSet::SetRowsAffected(int64_t count) {
  rows_affected_ = count;
  is_valid_ = true;
}

}  // namespace blink
