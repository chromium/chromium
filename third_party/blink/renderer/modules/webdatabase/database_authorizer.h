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
#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBDATABASE_DATABASE_AUTHORIZER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBDATABASE_DATABASE_AUTHORIZER_H_

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

extern const int kSQLAuthAllow;
extern const int kSQLAuthDeny;

class DatabaseAuthorizer {
  DISALLOW_NEW();

 public:
  enum Permissions {
    kReadWriteMask = 0,
    kReadOnlyMask = 1 << 1,
    kNoAccessMask = 1 << 2
  };

  explicit DatabaseAuthorizer(const String& database_info_table_name);
  ~DatabaseAuthorizer();

  int CreateTable(const String& table_name);
  int CreateTempTable(const String& table_name);
  int DropTable(const String& table_name);
  int DropTempTable(const String& table_name);
  int AllowAlterTable(const String& database_name, const String& table_name);

  int CreateIndex(const String& index_name, const String& table_name);
  int CreateTempIndex(const String& index_name, const String& table_name);
  int DropIndex(const String& index_name, const String& table_name);
  int DropTempIndex(const String& index_name, const String& table_name);

  int CreateTrigger(const String& trigger_name, const String& table_name);
  int CreateTempTrigger(const String& trigger_name, const String& table_name);
  int DropTrigger(const String& trigger_name, const String& table_name);
  int DropTempTrigger(const String& trigger_name, const String& table_name);

  int CreateView(const String& view_name);
  int CreateTempView(const String& view_name);
  int DropView(const String& view_name);
  int DropTempView(const String& view_name);

  int CreateVTable(const String& table_name, const String& module_name);
  int DropVTable(const String& table_name, const String& module_name);

  int AllowDelete(const String& table_name);
  int AllowInsert(const String& table_name);
  int AllowUpdate(const String& table_name, const String& column_name);
  int AllowTransaction();

  int AllowSelect() { return kSQLAuthAllow; }
  int AllowRead(const String& table_name, const String& column_name);

  int AllowReindex(const String& index_name);
  int AllowAnalyze(const String& table_name);
  int AllowFunction(const String& function_name);
  int AllowPragma(const String& pragma_name, const String& first_argument);

  void Disable();
  void Enable();
  void SetPermissions(int permissions);

  void Reset();
  void ResetDeletes();

  bool LastActionWasInsert() const { return last_action_was_insert_; }
  bool LastActionChangedDatabase() const {
    return last_action_changed_database_;
  }
  bool HadDeletes() const { return had_deletes_; }

 private:
  int DenyBasedOnTableName(const String&) const;
  int UpdateDeletesBasedOnTableName(const String&);
  bool AllowWrite();

  int permissions_;
  bool security_enabled_ : 1;
  bool last_action_was_insert_ : 1;
  bool last_action_changed_database_ : 1;
  bool had_deletes_ : 1;

  const String database_info_table_name_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBDATABASE_DATABASE_AUTHORIZER_H_
