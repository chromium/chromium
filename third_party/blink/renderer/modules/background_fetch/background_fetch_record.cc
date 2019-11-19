// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/background_fetch/background_fetch_record.h"
#include "third_party/blink/renderer/core/fetch/request.h"
#include "third_party/blink/renderer/core/fetch/response.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

BackgroundFetchRecord::BackgroundFetchRecord(Request* request,
                                             ScriptState* script_state)
    : request_(request), script_state_(script_state) {
  DCHECK(request_);
  DCHECK(script_state_);

  response_ready_property_ = MakeGarbageCollected<ResponseReadyProperty>(
      ExecutionContext::From(script_state), this,
      ResponseReadyProperty::kResponseReady);
}

BackgroundFetchRecord::~BackgroundFetchRecord() = default;

void BackgroundFetchRecord::ResolveResponseReadyProperty(Response* response) {
  if (response_ready_property_->GetState() !=
      ScriptPromisePropertyBase::State::kPending) {
    return;
  }

  switch (record_state_) {
    case State::kPending:
      return;
    case State::kAborted:
      response_ready_property_->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kAbortError,
          "The fetch was aborted before the record was processed."));
      return;
    case State::kSettled:
      if (response) {
        response_ready_property_->Resolve(response);
        return;
      }

      if (!script_state_->ContextIsValid())
        return;

      // TODO(crbug.com/875201): Per https://wicg.github.io/background-fetch/
      // #background-fetch-response-exposed, this should be resolved with a
      // TypeError. Figure out a way to do so.
      // Rejecting this with a TypeError here doesn't work because the
      // RejectedType is a DOMException. Update this with the correct error
      // once confirmed, or change the RejectedType.
      response_ready_property_->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kUnknownError, "The response is not available."));
  }
}

ScriptPromise BackgroundFetchRecord::responseReady(ScriptState* script_state) {
  return response_ready_property_->Promise(script_state->World());
}

Request* BackgroundFetchRecord::request() const {
  return request_;
}

void BackgroundFetchRecord::UpdateState(
    BackgroundFetchRecord::State updated_state) {
  DCHECK_EQ(record_state_, State::kPending);

  if (!script_state_->ContextIsValid())
    return;
  record_state_ = updated_state;
  ResolveResponseReadyProperty(/* updated_response = */ nullptr);
}

void BackgroundFetchRecord::SetResponseAndUpdateState(
    mojom::blink::FetchAPIResponsePtr& response) {
  DCHECK(record_state_ == State::kPending);
  DCHECK(!response.is_null());

  if (!script_state_->ContextIsValid())
    return;
  record_state_ = State::kSettled;

  ScriptState::Scope scope(script_state_);
  ResolveResponseReadyProperty(Response::Create(script_state_, *response));
}

bool BackgroundFetchRecord::IsRecordPending() {
  return record_state_ == State::kPending;
}

void BackgroundFetchRecord::OnRequestCompleted(
    mojom::blink::FetchAPIResponsePtr response) {
  if (!response.is_null())
    SetResponseAndUpdateState(response);
  else
    UpdateState(State::kSettled);
}

const KURL& BackgroundFetchRecord::ObservedUrl() const {
  return request_->url();
}

void BackgroundFetchRecord::Trace(blink::Visitor* visitor) {
  visitor->Trace(request_);
  visitor->Trace(response_ready_property_);
  visitor->Trace(script_state_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
