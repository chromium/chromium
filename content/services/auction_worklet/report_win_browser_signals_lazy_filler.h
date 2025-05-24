// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_REPORT_WIN_BROWSER_SIGNALS_LAZY_FILLER_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_REPORT_WIN_BROWSER_SIGNALS_LAZY_FILLER_H_

#include <optional>

#include "base/functional/callback.h"
#include "content/common/content_export.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/context_recycler.h"
#include "v8/include/v8-forward.h"

namespace auction_worklet {

class CONTENT_EXPORT ReportWinBrowserSignalsLazyFiller
    : public PersistedLazyFiller {
 public:
  // `v8_helper` must outlive `this`.
  explicit ReportWinBrowserSignalsLazyFiller(AuctionV8Helper* v8_helper);

  // Returns success/failure.
  bool FillInObject(
      const std::optional<uint16_t> browser_signal_modeling_signals,
      const uint8_t browser_signal_join_count,
      const std::optional<uint8_t> browser_signal_recency,
      v8::Local<v8::Object> object);

  void Reset() override;

  // Returns whether or not `sendEncryptedTo()` should be allowed within
  // `reportAggregateWin()`.
  bool ShouldBlockSendEncrypted() { return should_block_send_encrypted_; }

 private:
  // Handles the setting the browser signals: `modelingSignals`, `joinCount`,
  // and `recency`.
  static void HandleModelingSignals(
      v8::Local<v8::Name> name,
      const v8::PropertyCallbackInfo<v8::Value>& info);
  static void HandleJoinCount(v8::Local<v8::Name> name,
                              const v8::PropertyCallbackInfo<v8::Value>& info);
  static void HandleRecency(v8::Local<v8::Name> name,
                            const v8::PropertyCallbackInfo<v8::Value>& info);

  // These fields are optional to help determine whether we should define the
  // lazy attribute for each of these, as well as simplifying how we reset them.
  std::optional<uint16_t> browser_signal_modeling_signals_ = std::nullopt;
  std::optional<uint8_t> browser_signal_join_count_ = std::nullopt;
  std::optional<uint8_t> browser_signal_recency_ = std::nullopt;

  // The Private Model Training api does not permit the use of
  // `sendEncryptedTo()` if `modelingSignals`, `joinCount`, or `recency` were
  // accessed. This will keep track whether we should block the use of
  // `sendEncryptedTo()` based on whether those fields were accessed.
  bool should_block_send_encrypted_ = false;
};

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_REPORT_WIN_BROWSER_SIGNALS_LAZY_FILLER_H_
