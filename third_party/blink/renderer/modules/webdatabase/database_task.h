/*
 * Copyright (C) 2007, 2008, 2013 Apple Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBDATABASE_DATABASE_TASK_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBDATABASE_DATABASE_TASK_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/synchronization/waitable_event.h"
#include "third_party/blink/renderer/modules/webdatabase/database.h"
#include "third_party/blink/renderer/modules/webdatabase/database_basic_types.h"
#include "third_party/blink/renderer/modules/webdatabase/database_error.h"
#include "third_party/blink/renderer/modules/webdatabase/sql_transaction_backend.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/threading.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class DatabaseTask {
  USING_FAST_MALLOC(DatabaseTask);

 public:
  virtual ~DatabaseTask();

  void Run();

  Database* GetDatabase() const { return database_.Get(); }

 protected:
  DatabaseTask(Database*, base::WaitableEvent* complete_event);

 private:
  virtual void DoPerformTask() = 0;
  virtual void TaskCancelled() {}

  CrossThreadPersistent<Database> database_;
  base::WaitableEvent* complete_event_;

#if DCHECK_IS_ON()
  virtual const char* DebugTaskName() const = 0;
  bool complete_;
#endif

  DISALLOW_COPY_AND_ASSIGN(DatabaseTask);
};

class Database::DatabaseOpenTask final : public DatabaseTask {
 public:
  DatabaseOpenTask(Database*,
                   bool set_version_in_new_database,
                   base::WaitableEvent*,
                   DatabaseError&,
                   String& error_message,
                   bool& success);

 private:
  void DoPerformTask() override;
#if DCHECK_IS_ON()
  const char* DebugTaskName() const override;
#endif

  bool set_version_in_new_database_;
  DatabaseError& error_;
  String& error_message_;
  bool& success_;
};

class Database::DatabaseCloseTask final : public DatabaseTask {
 public:
  DatabaseCloseTask(Database*, base::WaitableEvent*);

 private:
  void DoPerformTask() override;
#if DCHECK_IS_ON()
  const char* DebugTaskName() const override;
#endif
};

class Database::DatabaseTransactionTask final : public DatabaseTask {
 public:
  // Transaction task is never synchronous, so no 'synchronizer' parameter.
  explicit DatabaseTransactionTask(SQLTransactionBackend*);
  ~DatabaseTransactionTask() override;

  SQLTransactionBackend* Transaction() const { return transaction_.Get(); }

 private:
  void DoPerformTask() override;
  void TaskCancelled() override;
#if DCHECK_IS_ON()
  const char* DebugTaskName() const override;
#endif

  CrossThreadPersistent<SQLTransactionBackend> transaction_;
};

class Database::DatabaseTableNamesTask final : public DatabaseTask {
 public:
  DatabaseTableNamesTask(Database*,
                         base::WaitableEvent*,
                         Vector<String>& names);

 private:
  void DoPerformTask() override;
#if DCHECK_IS_ON()
  const char* DebugTaskName() const override;
#endif

  Vector<String>& table_names_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBDATABASE_DATABASE_TASK_H_
