// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/reporting_observer.h"

#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/report.h"
#include "third_party/blink/renderer/core/frame/reporting_context.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

ReportingObserver* ReportingObserver::Create(
    ExecutionContext* execution_context,
    V8ReportingObserverCallback* callback,
    ReportingObserverOptions* options) {
  return MakeGarbageCollected<ReportingObserver>(execution_context, callback,
                                                 options);
}

ReportingObserver::ReportingObserver(ExecutionContext* execution_context,
                                     V8ReportingObserverCallback* callback,
                                     ReportingObserverOptions* options)
    : ActiveScriptWrappable<ReportingObserver>({}),
      ExecutionContextClient(execution_context),
      execution_context_(execution_context),
      callback_(callback),
      options_(options),
      registered_(false) {}

bool ReportingObserver::HasPendingActivity() const {
  return registered_;
}

void ReportingObserver::ReportToCallback() {
  // The reports queued to be sent to callbacks are copied (and cleared) before
  // being sent, since additional reports may be queued as a result of the
  // callbacks.
  auto reports_to_send = report_queue_;
  report_queue_.clear();
  callback_->InvokeAndReportException(this, reports_to_send, this);
}

void ReportingObserver::QueueReport(Report* report) {
  if (!ObservedType(report->type()))
    return;

  report_queue_.push_back(report);

  // When the first report of a batch is queued, make a task to report the whole
  // batch.
  if (report_queue_.size() == 1) {
    execution_context_->GetTaskRunner(TaskType::kMiscPlatformAPI)
        ->PostTask(FROM_HERE,
                   WTF::BindOnce(&ReportingObserver::ReportToCallback,
                                 WrapWeakPersistent(this)));
  }
}

bool ReportingObserver::ObservedType(const String& type) {
  return !options_->hasTypesNonNull() || options_->typesNonNull().empty() ||
         options_->typesNonNull().Find(type) != kNotFound;
}

bool ReportingObserver::Buffered() {
  return options_->hasBuffered() && options_->buffered();
}

void ReportingObserver::ClearBuffered() {
  return options_->setBuffered(false);
}

void ReportingObserver::observe() {
  registered_ = true;
  ReportingContext::From(execution_context_)->RegisterObserver(this);
}

void ReportingObserver::disconnect() {
  registered_ = false;
  ReportingContext::From(execution_context_)->UnregisterObserver(this);
}

HeapVector<Member<Report>> ReportingObserver::takeRecords() {
  auto reports = report_queue_;
  report_queue_.clear();
  return reports;
}

void ReportingObserver::Trace(Visitor* visitor) const {
  visitor->Trace(execution_context_);
  visitor->Trace(callback_);
  visitor->Trace(options_);
  visitor->Trace(report_queue_);
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

}  // namespace blink
