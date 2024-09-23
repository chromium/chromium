/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBDATABASE_SQL_TRANSACTION_STATE_MACHINE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBDATABASE_SQL_TRANSACTION_STATE_MACHINE_H_

#include "base/check_op.h"
#include "base/dcheck_is_on.h"
#include "third_party/blink/renderer/modules/webdatabase/sql_transaction_state.h"

namespace blink {

template <typename T>
class SQLTransactionStateMachine {
 public:
  virtual ~SQLTransactionStateMachine() = default;

 protected:
  SQLTransactionStateMachine();

  typedef SQLTransactionState (T::*StateFunction)();
  virtual StateFunction StateFunctionFor(SQLTransactionState) = 0;

  void SetStateToRequestedState();
  void RunStateMachine();

  SQLTransactionState next_state_;
  SQLTransactionState requested_state_;

#if DCHECK_IS_ON()
  // The state audit trail (i.e. bread crumbs) keeps track of up to the last
  // s_sizeOfStateAuditTrail states that the state machine enters. The audit
  // trail is updated before entering each state. This is for debugging use
  // only.
  static const int kSizeOfStateAuditTrail = 20;
  int next_state_audit_entry_ = 0;
  SQLTransactionState state_audit_trail_[kSizeOfStateAuditTrail];
#endif
};

#if DCHECK_IS_ON()
extern const char* NameForSQLTransactionState(SQLTransactionState);
#endif

template <typename T>
SQLTransactionStateMachine<T>::SQLTransactionStateMachine()
    : next_state_(SQLTransactionState::kIdle),
      requested_state_(SQLTransactionState::kIdle) {
#if DCHECK_IS_ON()
  for (int i = 0; i < kSizeOfStateAuditTrail; i++)
    state_audit_trail_[i] = SQLTransactionState::kNumberOfStates;
#endif
}

template <typename T>
void SQLTransactionStateMachine<T>::SetStateToRequestedState() {
  DCHECK_EQ(next_state_, SQLTransactionState::kIdle);
  DCHECK_NE(requested_state_, SQLTransactionState::kIdle);
  next_state_ = requested_state_;
  requested_state_ = SQLTransactionState::kIdle;
}

template <typename T>
void SQLTransactionStateMachine<T>::RunStateMachine() {
  DCHECK_LT(SQLTransactionState::kEnd, SQLTransactionState::kIdle);
  while (next_state_ > SQLTransactionState::kIdle) {
    DCHECK_LT(next_state_, SQLTransactionState::kNumberOfStates);
    StateFunction state_function = StateFunctionFor(next_state_);
    DCHECK(state_function);

#if DCHECK_IS_ON()
    state_audit_trail_[next_state_audit_entry_] = next_state_;
    next_state_audit_entry_ =
        (next_state_audit_entry_ + 1) % kSizeOfStateAuditTrail;
#endif
    next_state_ = (static_cast<T*>(this)->*state_function)();
  }
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBDATABASE_SQL_TRANSACTION_STATE_MACHINE_H_
