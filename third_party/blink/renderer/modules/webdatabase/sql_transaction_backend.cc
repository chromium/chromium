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

#include "third_party/blink/renderer/modules/webdatabase/sql_transaction_backend.h"

#include <memory>

#include "base/stl_util.h"
#include "third_party/blink/renderer/modules/webdatabase/database.h"
#include "third_party/blink/renderer/modules/webdatabase/database_authorizer.h"
#include "third_party/blink/renderer/modules/webdatabase/database_context.h"
#include "third_party/blink/renderer/modules/webdatabase/database_thread.h"
#include "third_party/blink/renderer/modules/webdatabase/database_tracker.h"
#include "third_party/blink/renderer/modules/webdatabase/sql_error.h"
#include "third_party/blink/renderer/modules/webdatabase/sql_statement_backend.h"
#include "third_party/blink/renderer/modules/webdatabase/sql_transaction.h"
#include "third_party/blink/renderer/modules/webdatabase/sql_transaction_client.h"
#include "third_party/blink/renderer/modules/webdatabase/sql_transaction_coordinator.h"
#include "third_party/blink/renderer/modules/webdatabase/sqlite/sql_value.h"
#include "third_party/blink/renderer/modules/webdatabase/sqlite/sqlite_transaction.h"
#include "third_party/blink/renderer/modules/webdatabase/storage_log.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

// How does a SQLTransaction work?
// ==============================
// The SQLTransaction is a state machine that executes a series of states /
// steps.
//
// The work of the transaction states are defined in section of 4.3.2 of the
// webdatabase spec: http://dev.w3.org/html5/webdatabase/#processing-model
//
// the State Transition Graph at a glance:
// ======================================
//
//     Backend                        .   Frontend
//     (works with SQLiteDatabase)    .   (works with Script)
//     ===========================    .   ===================
//                                    .
//     1. Idle                        .
//         v                          .
//     2. AcquireLock                 .
//         v                          .
//     3. OpenTransactionAndPreflight -----------------------------------.
//         |                        .                                    |
//         `-------------------------> 8. DeliverTransactionCallback --. |
//                                  .     |                           v v
//         ,------------------------------' 9. DeliverTransactionErrorCallback +
//         |                        .                                  ^ ^ ^   |
//         v                        .                                  | | |   |
//     4. RunStatements -----------------------------------------------' | |   |
//         |        ^  ^ |  ^ |     .                                    | |   |
//         |--------'  | |  | `------> 10. DeliverStatementCallback +----' |   |
//         |           | |  `---------------------------------------'      |   |
//         |           | `-----------> 11. DeliverQuotaIncreaseCallback +  |   |
//         |            `-----------------------------------------------'  |   |
//         v                        .                                      |   |
//     5. PostflightAndCommit --+------------------------------------------'   |
//                              |----> 12. DeliverSuccessCallback +            |
//         ,--------------------'   .                             |            |
//         v                        .                             |            |
//     6. CleanupAndTerminate <-----------------------------------'            |
//         v           ^            .                                          |
//     0. End          |            .                                          |
//                     |            .                                          |
//                7: CleanupAfterTransactionErrorCallback <--------------------'
//                                  .
//
// the States and State Transitions:
// ================================
//     0. SQLTransactionState::End
//         - the end state.
//
//     1. SQLTransactionState::Idle
//         - placeholder state while waiting on frontend/backend, etc. See
//           comment on "State transitions between SQLTransaction and
//           SQLTransactionBackend" below.
//
//     2. SQLTransactionState::AcquireLock (runs in backend)
//         - this is the start state.
//         - acquire the "lock".
//         - on "lock" acquisition, goto
//           SQLTransactionState::OpenTransactionAndPreflight.
//
//     3. SQLTransactionState::openTransactionAndPreflight (runs in backend)
//         - Sets up an SQLiteTransaction.
//         - begin the SQLiteTransaction.
//         - call the SQLTransactionWrapper preflight if available.
//         - schedule script callback.
//         - on error, goto
//           SQLTransactionState::DeliverTransactionErrorCallback.
//         - goto SQLTransactionState::DeliverTransactionCallback.
//
//     4. SQLTransactionState::DeliverTransactionCallback (runs in frontend)
//         - invoke the script function callback() if available.
//         - on error, goto
//           SQLTransactionState::DeliverTransactionErrorCallback.
//         - goto SQLTransactionState::RunStatements.
//
//     5. SQLTransactionState::DeliverTransactionErrorCallback (runs in
//        frontend)
//         - invoke the script function errorCallback if available.
//         - goto SQLTransactionState::CleanupAfterTransactionErrorCallback.
//
//     6. SQLTransactionState::RunStatements (runs in backend)
//         - while there are statements {
//             - run a statement.
//             - if statementCallback is available, goto
//               SQLTransactionState::DeliverStatementCallback.
//             - on error,
//               goto SQLTransactionState::DeliverQuotaIncreaseCallback, or
//               goto SQLTransactionState::DeliverStatementCallback, or
//               goto SQLTransactionState::deliverTransactionErrorCallback.
//           }
//         - goto SQLTransactionState::PostflightAndCommit.
//
//     7. SQLTransactionState::DeliverStatementCallback (runs in frontend)
//         - invoke script statement callback (assume available).
//         - on error, goto
//           SQLTransactionState::DeliverTransactionErrorCallback.
//         - goto SQLTransactionState::RunStatements.
//
//     8. SQLTransactionState::DeliverQuotaIncreaseCallback (runs in frontend)
//         - give client a chance to increase the quota.
//         - goto SQLTransactionState::RunStatements.
//
//     9. SQLTransactionState::PostflightAndCommit (runs in backend)
//         - call the SQLTransactionWrapper postflight if available.
//         - commit the SQLiteTansaction.
//         - on error, goto
//           SQLTransactionState::DeliverTransactionErrorCallback.
//         - if successCallback is available, goto
//           SQLTransactionState::DeliverSuccessCallback.
//           else goto SQLTransactionState::CleanupAndTerminate.
//
//     10. SQLTransactionState::DeliverSuccessCallback (runs in frontend)
//         - invoke the script function successCallback() if available.
//         - goto SQLTransactionState::CleanupAndTerminate.
//
//     11. SQLTransactionState::CleanupAndTerminate (runs in backend)
//         - stop and clear the SQLiteTransaction.
//         - release the "lock".
//         - goto SQLTransactionState::End.
//
//     12. SQLTransactionState::CleanupAfterTransactionErrorCallback (runs in
//         backend)
//         - rollback the SQLiteTransaction.
//         - goto SQLTransactionState::CleanupAndTerminate.
//
// State transitions between SQLTransaction and SQLTransactionBackend
// ==================================================================
// As shown above, there are state transitions that crosses the boundary between
// the frontend and backend. For example,
//
//     OpenTransactionAndPreflight (state 3 in the backend)
//     transitions to DeliverTransactionCallback (state 8 in the frontend),
//     which in turn transitions to RunStatements (state 4 in the backend).
//
// This cross boundary transition is done by posting transition requests to the
// other side and letting the other side's state machine execute the state
// transition in the appropriate thread (i.e. the script thread for the
// frontend, and the database thread for the backend).
//
// Logically, the state transitions work as shown in the graph above. But
// physically, the transition mechanism uses the Idle state (both in the
// frontend and backend) as a waiting state for further activity. For example,
// taking a closer look at the 3 state transition example above, what actually
// happens is as follows:
//
//     Step 1:
//     ======
//     In the frontend thread:
//     - waiting quietly is Idle. Not doing any work.
//
//     In the backend:
//     - is in OpenTransactionAndPreflight, and doing its work.
//     - when done, it transits to the backend DeliverTransactionCallback.
//     - the backend DeliverTransactionCallback sends a request to the frontend
//       to transit to DeliverTransactionCallback, and then itself transits to
//       Idle.
//
//     Step 2:
//     ======
//     In the frontend thread:
//     - transits to DeliverTransactionCallback and does its work.
//     - when done, it transits to the frontend RunStatements.
//     - the frontend RunStatements sends a request to the backend to transit
//       to RunStatements, and then itself transits to Idle.
//
//     In the backend:
//     - waiting quietly in Idle.
//
//     Step 3:
//     ======
//     In the frontend thread:
//     - waiting quietly is Idle. Not doing any work.
//
//     In the backend:
//     - transits to RunStatements, and does its work.
//        ...
//
// So, when the frontend or backend are not active, they will park themselves in
// their Idle states. This means their m_nextState is set to Idle, but they
// never actually run the corresponding state function. Note: for both the
// frontend and backend, the state function for Idle is unreachableState().
//
// The states that send a request to their peer across the front/back boundary
// are implemented with just 2 functions: SQLTransaction::sendToBackendState()
// and SQLTransactionBackend::sendToFrontendState(). These state functions do
// nothing but sends a request to the other side to transit to the current
// state (indicated by m_nextState), and then transits itself to the Idle state
// to wait for further action.

// The Life-Cycle of a SQLTransaction i.e. Who's keeping the SQLTransaction
// alive?
// ==============================================================================
// The RefPtr chain goes something like this:
//
//     At birth (in Database::runTransaction()):
//     ====================================================
//     Database
//         // HeapDeque<Member<SQLTransactionBackend>> m_transactionQueue
//         // points to ...
//     --> SQLTransactionBackend
//         // Member<SQLTransaction> m_frontend points to ...
//     --> SQLTransaction
//         // Member<SQLTransactionBackend> m_backend points to ...
//     --> SQLTransactionBackend  // which is a circular reference.
//
//     Note: there's a circular reference between the SQLTransaction front-end
//     and back-end. This circular reference is established in the constructor
//     of the SQLTransactionBackend. The circular reference will be broken by
//     calling doCleanup() to nullify m_frontend. This is done at the end of the
//     transaction's clean up state (i.e. when the transaction should no longer
//     be in use thereafter), or if the database was interrupted. See comments
//     on "What happens if a transaction is interrupted?" below for details.
//
//     After scheduling the transaction with the DatabaseThread
//     (Database::scheduleTransaction()):
//     ======================================================================================================
//     DatabaseThread
//         // MessageQueue<DatabaseTask> m_queue points to ...
//     --> DatabaseTransactionTask
//         // Member<SQLTransactionBackend> m_transaction points to ...
//     --> SQLTransactionBackend
//         // Member<SQLTransaction> m_frontend points to ...
//     --> SQLTransaction
//         // Member<SQLTransactionBackend> m_backend points to ...
//     --> SQLTransactionBackend  // which is a circular reference.
//
//     When executing the transaction (in DatabaseThread::databaseThread()):
//     ====================================================================
//     std::unique_ptr<DatabaseTask> task;
//         // points to ...
//     --> DatabaseTransactionTask
//         // Member<SQLTransactionBackend> m_transaction points to ...
//     --> SQLTransactionBackend
//         // Member<SQLTransaction> m_frontend;
//     --> SQLTransaction
//         // Member<SQLTransactionBackend> m_backend points to ...
//     --> SQLTransactionBackend  // which is a circular reference.
//
//     At the end of cleanupAndTerminate():
//     ===================================
//     At the end of the cleanup state, the SQLTransactionBackend::m_frontend is
//     nullified.  If by then, a JSObject wrapper is referring to the
//     SQLTransaction, then the reference chain looks like this:
//
//     JSObjectWrapper
//     --> SQLTransaction
//         // in Member<SQLTransactionBackend> m_backend points to ...
//     --> SQLTransactionBackend
//         // which no longer points back to its SQLTransaction.
//
//     When the GC collects the corresponding JSObject, the above chain will be
//     cleaned up and deleted.
//
//     If there is no JSObject wrapper referring to the SQLTransaction when the
//     cleanup states nullify SQLTransactionBackend::m_frontend, the
//     SQLTransaction will deleted then.  However, there will still be a
//     DatabaseTask pointing to the SQLTransactionBackend (see the "When
//     executing the transaction" chain above). This will keep the
//     SQLTransactionBackend alive until DatabaseThread::databaseThread()
//     releases its task std::unique_ptr.
//
//     What happens if a transaction is interrupted?
//     ============================================
//     If the transaction is interrupted half way, it won't get to run to state
//     CleanupAndTerminate, and hence, would not have called
//     SQLTransactionBackend's doCleanup(). doCleanup() is where we nullify
//     SQLTransactionBackend::m_frontend to break the reference cycle between
//     the frontend and backend. Hence, we need to cleanup the transaction by
//     other means.
//
//     Note: calling SQLTransactionBackend::notifyDatabaseThreadIsShuttingDown()
//     is effectively the same as calling SQLTransactionBackend::doClean().
//
//     In terms of who needs to call doCleanup(), there are 5 phases in the
//     SQLTransactionBackend life-cycle. These are the phases and how the clean
//     up is done:
//
//     Phase 1. After Birth, before scheduling
//
//     - To clean up, DatabaseThread::databaseThread() will call
//       Database::close() during its shutdown.
//     - Database::close() will iterate
//       Database::m_transactionQueue and call
//       notifyDatabaseThreadIsShuttingDown() on each transaction there.
//
//     Phase 2. After scheduling, before state AcquireLock
//
//     - If the interruption occures before the DatabaseTransactionTask is
//       scheduled in DatabaseThread::m_queue but hasn't gotten to execute
//       (i.e. DatabaseTransactionTask::performTask() has not been called),
//       then the DatabaseTransactionTask may get destructed before it ever
//       gets to execute.
//     - To clean up, the destructor will check if the task's m_wasExecuted is
//       set. If not, it will call notifyDatabaseThreadIsShuttingDown() on
//       the task's transaction.
//
//     Phase 3. After state AcquireLock, before "lockAcquired"
//
//     - In this phase, the transaction would have been added to the
//       SQLTransactionCoordinator's CoordinationInfo's pendingTransactions.
//     - To clean up, during shutdown, DatabaseThread::databaseThread() calls
//       SQLTransactionCoordinator::shutdown(), which calls
//       notifyDatabaseThreadIsShuttingDown().
//
//     Phase 4: After "lockAcquired", before state CleanupAndTerminate
//
//     - In this phase, the transaction would have been added either to the
//       SQLTransactionCoordinator's CoordinationInfo's activeWriteTransaction
//       or activeReadTransactions.
//     - To clean up, during shutdown, DatabaseThread::databaseThread() calls
//       SQLTransactionCoordinator::shutdown(), which calls
//       notifyDatabaseThreadIsShuttingDown().
//
//     Phase 5: After state CleanupAndTerminate
//
//     - This is how a transaction ends normally.
//     - state CleanupAndTerminate calls doCleanup().

namespace blink {

SQLTransactionBackend::SQLTransactionBackend(Database* db,
                                             SQLTransaction* frontend,
                                             SQLTransactionWrapper* wrapper,
                                             bool read_only)
    : frontend_(frontend),
      database_(db),
      wrapper_(wrapper),
      has_callback_(frontend_->HasCallback()),
      has_success_callback_(frontend_->HasSuccessCallback()),
      has_error_callback_(frontend_->HasErrorCallback()),
      should_retry_current_statement_(false),
      modified_database_(false),
      lock_acquired_(false),
      read_only_(read_only),
      has_version_mismatch_(false) {
  DCHECK(IsMainThread());
  DCHECK(database_);
  frontend_->SetBackend(this);
  requested_state_ = SQLTransactionState::kAcquireLock;
}

SQLTransactionBackend::~SQLTransactionBackend() {
  DCHECK(!sqlite_transaction_);
}

void SQLTransactionBackend::Trace(blink::Visitor* visitor) {
  visitor->Trace(wrapper_);
}

void SQLTransactionBackend::DoCleanup() {
  if (!frontend_)
    return;
  // Break the reference cycle. See comment about the life-cycle above.
  frontend_ = nullptr;

  DCHECK(GetDatabase()
             ->GetDatabaseContext()
             ->GetDatabaseThread()
             ->IsDatabaseThread());

  MutexLocker locker(statement_mutex_);
  statement_queue_.clear();

  if (sqlite_transaction_) {
    // In the event we got here because of an interruption or error (i.e. if
    // the transaction is in progress), we should roll it back here. Clearing
    // m_sqliteTransaction invokes SQLiteTransaction's destructor which does
    // just that. We might as well do this unconditionally and free up its
    // resources because we're already terminating.
    sqlite_transaction_.reset();
  }

  // Release the lock on this database
  if (lock_acquired_)
    database_->TransactionCoordinator()->ReleaseLock(this);

  // Do some aggresive clean up here except for m_database.
  //
  // We can't clear m_database here because the frontend may asynchronously
  // invoke SQLTransactionBackend::requestTransitToState(), and that function
  // uses m_database to schedule a state transition. This may occur because
  // the frontend (being in another thread) may already be on the way to
  // requesting our next state before it detects an interruption.
  //
  // There is no harm in letting it finish making the request. It'll set
  // m_requestedState, but we won't execute a transition to that state because
  // we've already shut down the transaction.
  //
  // We also can't clear m_currentStatementBackend and m_transactionError.
  // m_currentStatementBackend may be accessed asynchronously by the
  // frontend's deliverStatementCallback() state. Similarly,
  // m_transactionError may be accessed by deliverTransactionErrorCallback().
  // This occurs if requests for transition to those states have already been
  // registered with the frontend just prior to a clean up request arriving.
  //
  // So instead, let our destructor handle their clean up since this
  // SQLTransactionBackend is guaranteed to not destruct until the frontend
  // is also destructing.

  wrapper_ = nullptr;
}

SQLStatement* SQLTransactionBackend::CurrentStatement() {
  return current_statement_backend_->GetFrontend();
}

SQLErrorData* SQLTransactionBackend::TransactionError() {
  return transaction_error_.get();
}

void SQLTransactionBackend::SetShouldRetryCurrentStatement(bool should_retry) {
  DCHECK(!should_retry_current_statement_);
  should_retry_current_statement_ = should_retry;
}

SQLTransactionBackend::StateFunction SQLTransactionBackend::StateFunctionFor(
    SQLTransactionState state) {
  static const StateFunction kStateFunctions[] = {
      &SQLTransactionBackend::UnreachableState,                      // 0. end
      &SQLTransactionBackend::UnreachableState,                      // 1. idle
      &SQLTransactionBackend::AcquireLock,                           // 2.
      &SQLTransactionBackend::OpenTransactionAndPreflight,           // 3.
      &SQLTransactionBackend::RunStatements,                         // 4.
      &SQLTransactionBackend::PostflightAndCommit,                   // 5.
      &SQLTransactionBackend::CleanupAndTerminate,                   // 6.
      &SQLTransactionBackend::CleanupAfterTransactionErrorCallback,  // 7.
      // 8. deliverTransactionCallback
      &SQLTransactionBackend::SendToFrontendState,
      // 9. deliverTransactionErrorCallback
      &SQLTransactionBackend::SendToFrontendState,
      // 10.  deliverStatementCallback
      &SQLTransactionBackend::SendToFrontendState,
      // 11. deliverQuotaIncreaseCallback
      &SQLTransactionBackend::SendToFrontendState,
      // 12. deliverSuccessCallback
      &SQLTransactionBackend::SendToFrontendState,
  };

  DCHECK(base::size(kStateFunctions) ==
         static_cast<int>(SQLTransactionState::kNumberOfStates));
  DCHECK_LT(state, SQLTransactionState::kNumberOfStates);

  return kStateFunctions[static_cast<int>(state)];
}

void SQLTransactionBackend::EnqueueStatementBackend(
    SQLStatementBackend* statement_backend) {
  DCHECK(IsMainThread());
  MutexLocker locker(statement_mutex_);
  statement_queue_.push_back(statement_backend);
}

void SQLTransactionBackend::ComputeNextStateAndCleanupIfNeeded() {
  DCHECK(GetDatabase()
             ->GetDatabaseContext()
             ->GetDatabaseThread()
             ->IsDatabaseThread());
  // Only honor the requested state transition if we're not supposed to be
  // cleaning up and shutting down:
  if (database_->Opened()) {
    SetStateToRequestedState();
    DCHECK(next_state_ == SQLTransactionState::kAcquireLock ||
           next_state_ == SQLTransactionState::kOpenTransactionAndPreflight ||
           next_state_ == SQLTransactionState::kRunStatements ||
           next_state_ == SQLTransactionState::kPostflightAndCommit ||
           next_state_ == SQLTransactionState::kCleanupAndTerminate ||
           next_state_ ==
               SQLTransactionState::kCleanupAfterTransactionErrorCallback);
#if DCHECK_IS_ON()
    STORAGE_DVLOG(1) << "State " << NameForSQLTransactionState(next_state_);
#endif
    return;
  }

  // If we get here, then we should be shutting down. Do clean up if needed:
  if (next_state_ == SQLTransactionState::kEnd)
    return;
  next_state_ = SQLTransactionState::kEnd;

  // If the database was stopped, don't do anything and cancel queued work
  STORAGE_DVLOG(1) << "Database was stopped or interrupted - cancelling work "
                      "for this transaction";

  // The current SQLite transaction should be stopped, as well
  if (sqlite_transaction_) {
    sqlite_transaction_->Stop();
    sqlite_transaction_.reset();
  }

  // Terminate the frontend state machine. This also gets the frontend to
  // call computeNextStateAndCleanupIfNeeded() and clear its wrappers
  // if needed.
  frontend_->RequestTransitToState(SQLTransactionState::kEnd);

  // Redirect to the end state to abort, clean up, and end the transaction.
  DoCleanup();
}

void SQLTransactionBackend::PerformNextStep() {
  ComputeNextStateAndCleanupIfNeeded();
  RunStateMachine();
}

void SQLTransactionBackend::ExecuteSQL(SQLStatement* statement,
                                       const String& sql_statement,
                                       const Vector<SQLValue>& arguments,
                                       int permissions) {
  DCHECK(IsMainThread());
  EnqueueStatementBackend(MakeGarbageCollected<SQLStatementBackend>(
      statement, sql_statement, arguments, permissions));
}

void SQLTransactionBackend::NotifyDatabaseThreadIsShuttingDown() {
  DCHECK(GetDatabase()
             ->GetDatabaseContext()
             ->GetDatabaseThread()
             ->IsDatabaseThread());

  // If the transaction is in progress, we should roll it back here, since this
  // is our last opportunity to do something related to this transaction on the
  // DB thread. Amongst other work, doCleanup() will clear m_sqliteTransaction
  // which invokes SQLiteTransaction's destructor, which will do the roll back
  // if necessary.
  DoCleanup();
}

SQLTransactionState SQLTransactionBackend::AcquireLock() {
  database_->TransactionCoordinator()->AcquireLock(this);
  return SQLTransactionState::kIdle;
}

void SQLTransactionBackend::LockAcquired() {
  lock_acquired_ = true;
  RequestTransitToState(SQLTransactionState::kOpenTransactionAndPreflight);
}

SQLTransactionState SQLTransactionBackend::OpenTransactionAndPreflight() {
  DCHECK(GetDatabase()
             ->GetDatabaseContext()
             ->GetDatabaseThread()
             ->IsDatabaseThread());
  DCHECK(!database_->SqliteDatabase().TransactionInProgress());
  DCHECK(lock_acquired_);

  STORAGE_DVLOG(1) << "Opening and preflighting transaction " << this;

  // Set the maximum usage for this transaction if this transactions is not
  // read-only.
  if (!read_only_)
    database_->SqliteDatabase().SetMaximumSize(database_->MaximumSize());

  DCHECK(!sqlite_transaction_);
  sqlite_transaction_ = std::make_unique<SQLiteTransaction>(
      database_->SqliteDatabase(), read_only_);

  database_->ResetDeletes();
  database_->DisableAuthorizer();
  sqlite_transaction_->begin();
  database_->EnableAuthorizer();

  // Spec 4.3.2.1+2: Open a transaction to the database, jumping to the error
  // callback if that fails.
  if (!sqlite_transaction_->InProgress()) {
    DCHECK(!database_->SqliteDatabase().TransactionInProgress());
    database_->ReportSqliteError(database_->SqliteDatabase().LastError());
    transaction_error_ = SQLErrorData::Create(
        SQLError::kDatabaseErr, "unable to begin transaction",
        database_->SqliteDatabase().LastError(),
        database_->SqliteDatabase().LastErrorMsg());
    sqlite_transaction_.reset();
    return NextStateForTransactionError();
  }

  // Note: We intentionally retrieve the actual version even with an empty
  // expected version.  In multi-process browsers, we take this opportunity to
  // update the cached value for the actual version. In single-process browsers,
  // this is just a map lookup.
  String actual_version;
  if (!database_->GetActualVersionForTransaction(actual_version)) {
    database_->ReportSqliteError(database_->SqliteDatabase().LastError());
    transaction_error_ =
        SQLErrorData::Create(SQLError::kDatabaseErr, "unable to read version",
                             database_->SqliteDatabase().LastError(),
                             database_->SqliteDatabase().LastErrorMsg());
    database_->DisableAuthorizer();
    sqlite_transaction_.reset();
    database_->EnableAuthorizer();
    return NextStateForTransactionError();
  }
  has_version_mismatch_ = !database_->ExpectedVersion().IsEmpty() &&
                          (database_->ExpectedVersion() != actual_version);

  // Spec 4.3.2.3: Perform preflight steps, jumping to the error callback if
  // they fail.
  if (wrapper_ && !wrapper_->PerformPreflight(this)) {
    database_->DisableAuthorizer();
    sqlite_transaction_.reset();
    database_->EnableAuthorizer();
    if (wrapper_->SqlError()) {
      transaction_error_ =
          std::make_unique<SQLErrorData>(*wrapper_->SqlError());
    } else {
      transaction_error_ = std::make_unique<SQLErrorData>(
          SQLError::kUnknownErr,
          "unknown error occurred during transaction preflight");
    }
    return NextStateForTransactionError();
  }

  // Spec 4.3.2.4: Invoke the transaction callback with the new SQLTransaction
  // object.
  if (has_callback_)
    return SQLTransactionState::kDeliverTransactionCallback;

  // If we have no callback to make, skip pass to the state after:
  return SQLTransactionState::kRunStatements;
}

SQLTransactionState SQLTransactionBackend::RunStatements() {
  DCHECK(GetDatabase()
             ->GetDatabaseContext()
             ->GetDatabaseThread()
             ->IsDatabaseThread());
  DCHECK(lock_acquired_);
  SQLTransactionState next_state;

  // If there is a series of statements queued up that are all successful and
  // have no associated SQLStatementCallback objects, then we can burn through
  // the queue.
  do {
    if (should_retry_current_statement_ &&
        !sqlite_transaction_->WasRolledBackBySqlite()) {
      should_retry_current_statement_ = false;
      // FIXME - Another place that needs fixing up after
      // <rdar://problem/5628468> is addressed.
      // See ::openTransactionAndPreflight() for discussion

      // Reset the maximum size here, as it was increased to allow us to retry
      // this statement.  m_shouldRetryCurrentStatement is set to true only when
      // a statement exceeds the quota, which can happen only in a read-write
      // transaction.  Therefore, there is no need to check here if the
      // transaction is read-write.
      database_->SqliteDatabase().SetMaximumSize(database_->MaximumSize());
    } else {
      // If the current statement has already been run, failed due to quota
      // constraints, and we're not retrying it, that means it ended in an
      // error. Handle it now.
      if (current_statement_backend_ &&
          current_statement_backend_->LastExecutionFailedDueToQuota()) {
        return NextStateForCurrentStatementError();
      }

      // Otherwise, advance to the next statement
      GetNextStatement();
    }
    next_state = RunCurrentStatementAndGetNextState();
  } while (next_state == SQLTransactionState::kRunStatements);

  return next_state;
}

void SQLTransactionBackend::GetNextStatement() {
  DCHECK(GetDatabase()
             ->GetDatabaseContext()
             ->GetDatabaseThread()
             ->IsDatabaseThread());
  current_statement_backend_ = nullptr;

  MutexLocker locker(statement_mutex_);
  if (!statement_queue_.IsEmpty())
    current_statement_backend_ = statement_queue_.TakeFirst();
}

SQLTransactionState
SQLTransactionBackend::RunCurrentStatementAndGetNextState() {
  if (!current_statement_backend_) {
    // No more statements to run. So move on to the next state.
    return SQLTransactionState::kPostflightAndCommit;
  }

  database_->ResetAuthorizer();

  if (has_version_mismatch_)
    current_statement_backend_->SetVersionMismatchedError(database_.Get());

  if (current_statement_backend_->Execute(database_.Get())) {
    if (database_->LastActionChangedDatabase()) {
      // Flag this transaction as having changed the database for later delegate
      // notification.
      modified_database_ = true;
    }

    if (current_statement_backend_->HasStatementCallback()) {
      return SQLTransactionState::kDeliverStatementCallback;
    }

    // If we get here, then the statement doesn't have a callback to invoke.
    // We can move on to the next statement. Hence, stay in this state.
    return SQLTransactionState::kRunStatements;
  }

  if (current_statement_backend_->LastExecutionFailedDueToQuota()) {
    return SQLTransactionState::kDeliverQuotaIncreaseCallback;
  }

  return NextStateForCurrentStatementError();
}

SQLTransactionState SQLTransactionBackend::NextStateForCurrentStatementError() {
  // Spec 4.3.2.6.6: error - Call the statement's error callback, but if there
  // was no error callback, or the transaction was rolled back, jump to the
  // transaction error callback.
  if (current_statement_backend_->HasStatementErrorCallback() &&
      !sqlite_transaction_->WasRolledBackBySqlite())
    return SQLTransactionState::kDeliverStatementCallback;

  if (current_statement_backend_->SqlError()) {
    transaction_error_ =
        std::make_unique<SQLErrorData>(*current_statement_backend_->SqlError());
  } else {
    transaction_error_ = std::make_unique<SQLErrorData>(
        SQLError::kDatabaseErr, "the statement failed to execute");
  }
  return NextStateForTransactionError();
}

SQLTransactionState SQLTransactionBackend::PostflightAndCommit() {
  DCHECK(lock_acquired_);

  // Spec 4.3.2.7: Perform postflight steps, jumping to the error callback if
  // they fail.
  if (wrapper_ && !wrapper_->PerformPostflight(this)) {
    if (wrapper_->SqlError()) {
      transaction_error_ =
          std::make_unique<SQLErrorData>(*wrapper_->SqlError());
    } else {
      transaction_error_ = std::make_unique<SQLErrorData>(
          SQLError::kUnknownErr,
          "unknown error occurred during transaction postflight");
    }
    return NextStateForTransactionError();
  }

  // Spec 4.3.2.7: Commit the transaction, jumping to the error callback if that
  // fails.
  DCHECK(sqlite_transaction_);

  database_->DisableAuthorizer();
  sqlite_transaction_->Commit();
  database_->EnableAuthorizer();

  // If the commit failed, the transaction will still be marked as "in progress"
  if (sqlite_transaction_->InProgress()) {
    if (wrapper_)
      wrapper_->HandleCommitFailedAfterPostflight(this);
    database_->ReportSqliteError(database_->SqliteDatabase().LastError());
    transaction_error_ = SQLErrorData::Create(
        SQLError::kDatabaseErr, "unable to commit transaction",
        database_->SqliteDatabase().LastError(),
        database_->SqliteDatabase().LastErrorMsg());
    return NextStateForTransactionError();
  }

  // Vacuum the database if anything was deleted.
  if (database_->HadDeletes())
    database_->IncrementalVacuumIfNeeded();

  // The commit was successful. If the transaction modified this database,
  // notify the delegates.
  if (modified_database_)
    database_->TransactionClient()->DidCommitWriteTransaction(GetDatabase());

  // Spec 4.3.2.8: Deliver success callback, if there is one.
  return SQLTransactionState::kDeliverSuccessCallback;
}

SQLTransactionState SQLTransactionBackend::CleanupAndTerminate() {
  DCHECK(lock_acquired_);

  // Spec 4.3.2.9: End transaction steps. There is no next step.
  STORAGE_DVLOG(1) << "Transaction " << this << " is complete";
  DCHECK(!database_->SqliteDatabase().TransactionInProgress());

  // Phase 5 cleanup. See comment on the SQLTransaction life-cycle above.
  DoCleanup();
  database_->InProgressTransactionCompleted();
  return SQLTransactionState::kEnd;
}

SQLTransactionState SQLTransactionBackend::NextStateForTransactionError() {
  DCHECK(transaction_error_);
  if (has_error_callback_)
    return SQLTransactionState::kDeliverTransactionErrorCallback;

  // No error callback, so fast-forward to the next state and rollback the
  // transaction.
  return SQLTransactionState::kCleanupAfterTransactionErrorCallback;
}

SQLTransactionState
SQLTransactionBackend::CleanupAfterTransactionErrorCallback() {
  DCHECK(lock_acquired_);

  STORAGE_DVLOG(1) << "Transaction " << this << " is complete with an error";
  database_->DisableAuthorizer();
  if (sqlite_transaction_) {
    // Spec 4.3.2.10: Rollback the transaction.
    sqlite_transaction_->Rollback();

    DCHECK(!database_->SqliteDatabase().TransactionInProgress());
    sqlite_transaction_.reset();
  }
  database_->EnableAuthorizer();

  DCHECK(!database_->SqliteDatabase().TransactionInProgress());

  return SQLTransactionState::kCleanupAndTerminate;
}

// requestTransitToState() can be called from the frontend. Hence, it should
// NOT be modifying SQLTransactionBackend in general. The only safe field to
// modify is m_requestedState which is meant for this purpose.
void SQLTransactionBackend::RequestTransitToState(
    SQLTransactionState next_state) {
#if DCHECK_IS_ON()
  STORAGE_DVLOG(1) << "Scheduling " << NameForSQLTransactionState(next_state)
                   << " for transaction " << this;
#endif
  requested_state_ = next_state;
  DCHECK_NE(requested_state_, SQLTransactionState::kEnd);
  database_->ScheduleTransactionStep(this);
}

// This state function is used as a stub function to plug unimplemented states
// in the state dispatch table. They are unimplemented because they should
// never be reached in the course of correct execution.
SQLTransactionState SQLTransactionBackend::UnreachableState() {
  NOTREACHED();
  return SQLTransactionState::kEnd;
}

SQLTransactionState SQLTransactionBackend::SendToFrontendState() {
  DCHECK_NE(next_state_, SQLTransactionState::kIdle);
  frontend_->RequestTransitToState(next_state_);
  return SQLTransactionState::kIdle;
}

}  // namespace blink
