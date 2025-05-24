// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/report_win_browser_signals_lazy_filler.h"

#include <optional>

#include "content/services/auction_worklet/auction_v8_helper.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-primitive.h"

namespace auction_worklet {

ReportWinBrowserSignalsLazyFiller::ReportWinBrowserSignalsLazyFiller(
    AuctionV8Helper* v8_helper)
    : PersistedLazyFiller(v8_helper) {}

bool ReportWinBrowserSignalsLazyFiller::FillInObject(
    const std::optional<uint16_t> browser_signal_modeling_signals,
    uint8_t browser_signal_join_count,
    const std::optional<uint8_t> browser_signal_recency,
    v8::Local<v8::Object> object) {
  if (browser_signal_modeling_signals.has_value()) {
    browser_signal_modeling_signals_ = browser_signal_modeling_signals.value();
  }

  if (browser_signal_recency.has_value()) {
    browser_signal_recency_ = browser_signal_recency.value();
  }

  browser_signal_join_count_ = browser_signal_join_count;

  if (browser_signal_modeling_signals.has_value() &&
      !DefineLazyAttribute(object, "modelingSignals", &HandleModelingSignals)) {
    return false;
  }

  if (browser_signal_recency.has_value() &&
      !DefineLazyAttribute(object, "recency", &HandleRecency)) {
    return false;
  }

  return (DefineLazyAttribute(object, "joinCount", &HandleJoinCount));
}

void ReportWinBrowserSignalsLazyFiller::Reset() {
  browser_signal_modeling_signals_ = std::nullopt;
  browser_signal_join_count_ = std::nullopt;
  browser_signal_recency_ = std::nullopt;
  should_block_send_encrypted_ = false;
}

// static
void ReportWinBrowserSignalsLazyFiller::HandleModelingSignals(
    v8::Local<v8::Name> name,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  ReportWinBrowserSignalsLazyFiller* self =
      GetSelf<ReportWinBrowserSignalsLazyFiller>(info);
  v8::Isolate* isolate = self->v8_helper()->isolate();
  self->should_block_send_encrypted_ = true;
  if (self->browser_signal_modeling_signals_.has_value()) {
    SetResult(info, v8::Integer::NewFromUnsigned(
                        isolate, *self->browser_signal_modeling_signals_));
  } else {
    SetResult(info, v8::Null(isolate));
  }
}

// static
void ReportWinBrowserSignalsLazyFiller::HandleJoinCount(
    v8::Local<v8::Name> name,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  ReportWinBrowserSignalsLazyFiller* self =
      GetSelf<ReportWinBrowserSignalsLazyFiller>(info);
  v8::Isolate* isolate = self->v8_helper()->isolate();
  self->should_block_send_encrypted_ = true;
  if (self->browser_signal_join_count_.has_value()) {
    SetResult(info, v8::Integer::NewFromUnsigned(
                        isolate, *self->browser_signal_join_count_));
  } else {
    SetResult(info, v8::Null(isolate));
  }
}

// static
void ReportWinBrowserSignalsLazyFiller::HandleRecency(
    v8::Local<v8::Name> name,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  ReportWinBrowserSignalsLazyFiller* self =
      GetSelf<ReportWinBrowserSignalsLazyFiller>(info);
  v8::Isolate* isolate = self->v8_helper()->isolate();
  self->should_block_send_encrypted_ = true;
  if (self->browser_signal_recency_.has_value()) {
    SetResult(info, v8::Integer::NewFromUnsigned(
                        isolate, *self->browser_signal_recency_));
  } else {
    SetResult(info, v8::Null(isolate));
  }
}

}  // namespace auction_worklet
