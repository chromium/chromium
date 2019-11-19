/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/clipboard/data_transfer_item.h"

#include "base/location.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/clipboard/data_object_item.h"
#include "third_party/blink/renderer/core/clipboard/data_transfer.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/probe/async_task_id.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

String DataTransferItem::kind() const {
  DEFINE_STATIC_LOCAL(const String, kind_string, ("string"));
  DEFINE_STATIC_LOCAL(const String, kind_file, ("file"));
  if (!data_transfer_->CanReadTypes())
    return String();
  switch (item_->Kind()) {
    case DataObjectItem::kStringKind:
      return kind_string;
    case DataObjectItem::kFileKind:
      return kind_file;
  }
  NOTREACHED();
  return String();
}

String DataTransferItem::type() const {
  if (!data_transfer_->CanReadTypes())
    return String();
  return item_->GetType();
}

void DataTransferItem::getAsString(ScriptState* script_state,
                                   V8FunctionStringCallback* callback) {
  if (!data_transfer_->CanReadData())
    return;
  if (!callback || item_->Kind() != DataObjectItem::kStringKind)
    return;

  ExecutionContext* context = ExecutionContext::From(script_state);
  auto task_id = std::make_unique<probe::AsyncTaskId>();
  probe::AsyncTaskScheduled(context, "DataTransferItem.getAsString",
                            task_id.get());
  context->GetTaskRunner(TaskType::kUserInteraction)
      ->PostTask(
          FROM_HERE,
          WTF::Bind(&DataTransferItem::RunGetAsStringTask, WrapPersistent(this),
                    WrapPersistent(context), WrapPersistent(callback),
                    item_->GetAsString(), std::move(task_id)));
}

File* DataTransferItem::getAsFile() const {
  if (!data_transfer_->CanReadData())
    return nullptr;

  return item_->GetAsFile();
}

DataTransferItem::DataTransferItem(DataTransfer* data_transfer,
                                   DataObjectItem* item)
    : data_transfer_(data_transfer), item_(item) {}

void DataTransferItem::RunGetAsStringTask(
    ExecutionContext* context,
    V8FunctionStringCallback* callback,
    const String& data,
    std::unique_ptr<probe::AsyncTaskId> task_id) {
  DCHECK(callback);
  probe::AsyncTask async_task(context, task_id.get());
  if (context)
    callback->InvokeAndReportException(nullptr, data);
}

void DataTransferItem::Trace(blink::Visitor* visitor) {
  visitor->Trace(data_transfer_);
  visitor->Trace(item_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
