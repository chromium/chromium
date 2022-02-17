// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_STATE_IMPL_LACROS_H_
#define CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_STATE_IMPL_LACROS_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "content/browser/accessibility/browser_accessibility_state_impl.h"

class CrosapiPrefObserver;

namespace content {

class BrowserAccessibilityStateImplLacros
    : public BrowserAccessibilityStateImpl {
 public:
  BrowserAccessibilityStateImplLacros();
  ~BrowserAccessibilityStateImplLacros() override;

  BrowserAccessibilityStateImplLacros(
      const BrowserAccessibilityStateImplLacros&) = delete;
  BrowserAccessibilityStateImplLacros& operator=(
      const BrowserAccessibilityStateImplLacros&) = delete;

 private:
  void OnSpokenFeedbackPrefChanged(base::Value value);

  std::unique_ptr<CrosapiPrefObserver> crosapi_pref_observer_;
  base::WeakPtrFactory<BrowserAccessibilityStateImplLacros> weak_ptr_factory_{
      this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_STATE_IMPL_LACROS_H_
