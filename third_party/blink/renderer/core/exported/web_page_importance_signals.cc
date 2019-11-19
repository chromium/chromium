// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/web_page_importance_signals.h"

#include "third_party/blink/public/web/web_view_client.h"

namespace blink {

void WebPageImportanceSignals::Reset() {
  had_form_interaction_ = false;
  if (observer_)
    observer_->PageImportanceSignalsChanged();
}

void WebPageImportanceSignals::SetHadFormInteraction() {
  had_form_interaction_ = true;
  if (observer_)
    observer_->PageImportanceSignalsChanged();
}

void WebPageImportanceSignals::OnCommitLoad() {
  Reset();
}

}  // namespace blink
