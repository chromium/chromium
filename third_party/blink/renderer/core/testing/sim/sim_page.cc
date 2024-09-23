// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/sim/sim_page.h"

#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"

namespace blink {

SimPage::SimPage() : page_(nullptr) {}

SimPage::~SimPage() = default;

void SimPage::SetPage(Page* page) {
  page_ = page;
}

void SimPage::SetFocused(bool value) {
  if (value)
    page_->GetFocusController().SetActive(true);
  page_->GetFocusController().SetFocused(value);
}

bool SimPage::IsFocused() const {
  return page_->GetFocusController().IsFocused();
}

void SimPage::SetActive(bool value) {
  page_->GetFocusController().SetActive(value);
}

bool SimPage::IsActive() const {
  return page_->GetFocusController().IsActive();
}

const VisualViewport& SimPage::GetVisualViewport() const {
  return page_->GetVisualViewport();
}

}  // namespace blink
