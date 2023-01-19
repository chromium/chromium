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

#include "third_party/blink/renderer/modules/webdatabase/database_authorizer.h"

#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/case_folding_hash.h"
#include "third_party/blink/renderer/platform/wtf/threading.h"

namespace blink {

DatabaseAuthorizer::DatabaseAuthorizer(const String& database_info_table_name)
    : security_enabled_(false),
      database_info_table_name_(database_info_table_name) {
  DCHECK(IsMainThread());

  Reset();
}

DatabaseAuthorizer::~DatabaseAuthorizer() = default;

void DatabaseAuthorizer::Reset() {
  last_action_was_insert_ = false;
  last_action_changed_database_ = false;
  permissions_ = kReadWriteMask;
}

void DatabaseAuthorizer::ResetDeletes() {
  had_deletes_ = false;
}

namespace {
using FunctionNameList = HashSet<String, CaseFoldingHashTraits<String>>;

const FunctionNameList& AllowedFunctions() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      FunctionNameList, list,
      ({
          // SQLite functions used to help implement some operations
          // ALTER TABLE helpers
          "sqlite_rename_column",
          "sqlite_rename_table",
          "sqlite_rename_test",
          "sqlite_rename_quotefix",
          // GLOB helpers
          "glob",
          // SQLite core functions
          "abs",
          "changes",
          "coalesce",
          "glob",
          "ifnull",
          "hex",
          "last_insert_rowid",
          "length",
          "like",
          "lower",
          "ltrim",
          "max",
          "min",
          "nullif",
          "quote",
          "replace",
          "round",
          "rtrim",
          "soundex",
          "sqlite_source_id",
          "sqlite_version",
          "substr",
          "total_changes",
          "trim",
          "typeof",
          "upper",
          "zeroblob",
          // SQLite date and time functions
          "date",
          "time",
          "datetime",
          "julianday",
          "strftime",
          // SQLite aggregate functions
          // max() and min() are already in the list
          "avg",
          "count",
          "group_concat",
          "sum",
          "total",
          // SQLite FTS functions
          "match",
          "snippet",
          "offsets",
          "optimize",
          // SQLite ICU functions
          // like(), lower() and upper() are already in the list
          "regexp",
          // Used internally by ALTER TABLE ADD COLUMN.
          "printf",
      }));
  return list;
}
}

int DatabaseAuthorizer::CreateTable(const String& table_name) {
  if (!AllowWrite())
    return kSQLAuthDeny;

  last_action_changed_database_ = true;
  return DenyBasedOnTableName(table_name);
}

int DatabaseAuthorizer::CreateTempTable(const String& table_name) {
  // SQLITE_CREATE_TEMP_TABLE results in a UPDATE operation, which is not
  // allowed in read-only transactions or private browsing, so we might as
  // well disallow SQLITE_CREATE_TEMP_TABLE in these cases
  if (!AllowWrite())
    return kSQLAuthDeny;

  return DenyBasedOnTableName(table_name);
}

int DatabaseAuthorizer::DropTable(const String& table_name) {
  if (!AllowWrite())
    return kSQLAuthDeny;

  return UpdateDeletesBasedOnTableName(table_name);
}

int DatabaseAuthorizer::DropTempTable(const String& table_name) {
  // SQLITE_DROP_TEMP_TABLE results in a DELETE operation, which is not
  // allowed in read-only transactions or private browsing, so we might as
  // well disallow SQLITE_DROP_TEMP_TABLE in these cases
  if (!AllowWrite())
    return kSQLAuthDeny;

  return UpdateDeletesBasedOnTableName(table_name);
}

int DatabaseAuthorizer::AllowAlterTable(const String&,
                                        const String& table_name) {
  if (!AllowWrite())
    return kSQLAuthDeny;

  last_action_changed_database_ = true;
  return DenyBasedOnTableName(table_name);
}

int DatabaseAuthorizer::CreateIndex(const String&, const String& table_name) {
  if (!AllowWrite())
    return kSQLAuthDeny;

  last_action_changed_database_ = true;
  return DenyBasedOnTableName(table_name);
}

int DatabaseAuthorizer::CreateTempIndex(const String&,
                                        const String& table_name) {
  // SQLITE_CREATE_TEMP_INDEX should result in a UPDATE or INSERT operation,
  // which is not allowed in read-only transactions or private browsing,
  // so we might as well disallow SQLITE_CREATE_TEMP_INDEX in these cases
  if (!AllowWrite())
    return kSQLAuthDeny;

  return DenyBasedOnTableName(table_name);
}

int DatabaseAuthorizer::DropIndex(const String&, const String& table_name) {
  if (!AllowWrite())
    return kSQLAuthDeny;

  return UpdateDeletesBasedOnTableName(table_name);
}

int DatabaseAuthorizer::DropTempIndex(const String&, const String& table_name) {
  // SQLITE_DROP_TEMP_INDEX should result in a DELETE operation, which is
  // not allowed in read-only transactions or private browsing, so we might
  // as well disallow SQLITE_DROP_TEMP_INDEX in these cases
  if (!AllowWrite())
    return kSQLAuthDeny;

  return UpdateDeletesBasedOnTableName(table_name);
}

int DatabaseAuthorizer::CreateTrigger(const String&, const String& table_name) {
  if (!AllowWrite())
    return kSQLAuthDeny;

  last_action_changed_database_ = true;
  return DenyBasedOnTableName(table_name);
}

int DatabaseAuthorizer::CreateTempTrigger(const String&,
                                          const String& table_name) {
  // SQLITE_CREATE_TEMP_TRIGGER results in a INSERT operation, which is not
  // allowed in read-only transactions or private browsing, so we might as
  // well disallow SQLITE_CREATE_TEMP_TRIGGER in these cases
  if (!AllowWrite())
    return kSQLAuthDeny;

  return DenyBasedOnTableName(table_name);
}

int DatabaseAuthorizer::DropTrigger(const String&, const String& table_name) {
  if (!AllowWrite())
    return kSQLAuthDeny;

  return UpdateDeletesBasedOnTableName(table_name);
}

int DatabaseAuthorizer::DropTempTrigger(const String&,
                                        const String& table_name) {
  // SQLITE_DROP_TEMP_TRIGGER results in a DELETE operation, which is not
  // allowed in read-only transactions or private browsing, so we might as
  // well disallow SQLITE_DROP_TEMP_TRIGGER in these cases
  if (!AllowWrite())
    return kSQLAuthDeny;

  return UpdateDeletesBasedOnTableName(table_name);
}

int DatabaseAuthorizer::CreateView(const String&) {
  return (!AllowWrite() ? kSQLAuthDeny : kSQLAuthAllow);
}

int DatabaseAuthorizer::CreateTempView(const String&) {
  // SQLITE_CREATE_TEMP_VIEW results in a UPDATE operation, which is not
  // allowed in read-only transactions or private browsing, so we might as
  // well disallow SQLITE_CREATE_TEMP_VIEW in these cases
  return (!AllowWrite() ? kSQLAuthDeny : kSQLAuthAllow);
}

int DatabaseAuthorizer::DropView(const String&) {
  if (!AllowWrite())
    return kSQLAuthDeny;

  had_deletes_ = true;
  return kSQLAuthAllow;
}

int DatabaseAuthorizer::DropTempView(const String&) {
  // SQLITE_DROP_TEMP_VIEW results in a DELETE operation, which is not
  // allowed in read-only transactions or private browsing, so we might as
  // well disallow SQLITE_DROP_TEMP_VIEW in these cases
  if (!AllowWrite())
    return kSQLAuthDeny;

  had_deletes_ = true;
  return kSQLAuthAllow;
}

int DatabaseAuthorizer::CreateVTable(const String& table_name,
                                     const String& module_name) {
  if (!AllowWrite())
    return kSQLAuthDeny;

  // Allow only the FTS3 extension
  if (!EqualIgnoringASCIICase(module_name, "fts3"))
    return kSQLAuthDeny;

  last_action_changed_database_ = true;
  return DenyBasedOnTableName(table_name);
}

int DatabaseAuthorizer::DropVTable(const String& table_name,
                                   const String& module_name) {
  if (!AllowWrite())
    return kSQLAuthDeny;

  // Allow only the FTS3 extension
  if (!DeprecatedEqualIgnoringCase(module_name, "fts3"))
    return kSQLAuthDeny;

  return UpdateDeletesBasedOnTableName(table_name);
}

int DatabaseAuthorizer::AllowDelete(const String& table_name) {
  if (!AllowWrite())
    return kSQLAuthDeny;

  return UpdateDeletesBasedOnTableName(table_name);
}

int DatabaseAuthorizer::AllowInsert(const String& table_name) {
  if (!AllowWrite())
    return kSQLAuthDeny;

  last_action_changed_database_ = true;
  last_action_was_insert_ = true;
  return DenyBasedOnTableName(table_name);
}

int DatabaseAuthorizer::AllowUpdate(const String& table_name, const String&) {
  if (!AllowWrite())
    return kSQLAuthDeny;

  last_action_changed_database_ = true;
  return DenyBasedOnTableName(table_name);
}

int DatabaseAuthorizer::AllowTransaction() {
  return security_enabled_ ? kSQLAuthDeny : kSQLAuthAllow;
}

int DatabaseAuthorizer::AllowRead(const String& table_name, const String&) {
  if (permissions_ & kNoAccessMask && security_enabled_)
    return kSQLAuthDeny;

  return DenyBasedOnTableName(table_name);
}

int DatabaseAuthorizer::AllowReindex(const String&) {
  return (!AllowWrite() ? kSQLAuthDeny : kSQLAuthAllow);
}

int DatabaseAuthorizer::AllowAnalyze(const String& table_name) {
  return DenyBasedOnTableName(table_name);
}

int DatabaseAuthorizer::AllowPragma(const String&, const String&) {
  return security_enabled_ ? kSQLAuthDeny : kSQLAuthAllow;
}

int DatabaseAuthorizer::AllowFunction(const String& function_name) {
  if (security_enabled_ && !AllowedFunctions().Contains(function_name))
    return kSQLAuthDeny;

  return kSQLAuthAllow;
}

void DatabaseAuthorizer::Disable() {
  security_enabled_ = false;
}

void DatabaseAuthorizer::Enable() {
  security_enabled_ = true;
}

bool DatabaseAuthorizer::AllowWrite() {
  return !(security_enabled_ &&
           (permissions_ & kReadOnlyMask || permissions_ & kNoAccessMask));
}

void DatabaseAuthorizer::SetPermissions(int permissions) {
  permissions_ = permissions;
}

int DatabaseAuthorizer::DenyBasedOnTableName(const String& table_name) const {
  if (!security_enabled_)
    return kSQLAuthAllow;

  // Sadly, normal creates and drops end up affecting sqlite_master in an
  // authorizer callback, so it will be tough to enforce all of the following
  // policies:
  // if (EqualIgnoringASCIICase(table_name, "sqlite_master") ||
  //     EqualIgnoringASCIICase(table_name, "sqlite_temp_master") ||
  //     EqualIgnoringASCIICase(table_name, "sqlite_sequence") ||
  //     EqualIgnoringASCIICase(table_name, database_info_table_name_))
  //   return SQLAuthDeny;

  if (EqualIgnoringASCIICase(table_name, database_info_table_name_))
    return kSQLAuthDeny;

  return kSQLAuthAllow;
}

int DatabaseAuthorizer::UpdateDeletesBasedOnTableName(
    const String& table_name) {
  int allow = DenyBasedOnTableName(table_name);
  if (allow)
    had_deletes_ = true;
  return allow;
}

}  // namespace blink
