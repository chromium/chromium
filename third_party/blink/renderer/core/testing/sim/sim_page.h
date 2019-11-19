// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_SIM_SIM_PAGE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_SIM_SIM_PAGE_H_

#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class Page;

class SimPage final {
 public:
  SimPage();
  ~SimPage();

  void SetPage(Page*);

  void SetFocused(bool);
  bool IsFocused() const;

 private:
  Persistent<Page> page_;
};

}  // namespace blink

#endif
