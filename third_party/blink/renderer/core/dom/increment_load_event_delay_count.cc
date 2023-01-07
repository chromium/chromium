// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/increment_load_event_delay_count.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/dom/document.h"

namespace blink {

IncrementLoadEventDelayCount::IncrementLoadEventDelayCount(Document& document)
    : document_(&document) {
  document.IncrementLoadEventDelayCount();
}

IncrementLoadEventDelayCount::~IncrementLoadEventDelayCount() {
  if (document_)
    document_->DecrementLoadEventDelayCount();
}

void IncrementLoadEventDelayCount::ClearAndCheckLoadEvent() {
  if (document_)
    document_->DecrementLoadEventDelayCountAndCheckLoadEvent();
  document_ = nullptr;
}

void IncrementLoadEventDelayCount::DocumentChanged(Document& new_document) {
  new_document.IncrementLoadEventDelayCount();
  if (document_)
    document_->DecrementLoadEventDelayCount();
  document_ = &new_document;
}
}  // namespace blink
