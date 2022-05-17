// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_MAIN_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_MAIN_ELEMENT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/html/html_element.h"

namespace blink {

class CORE_EXPORT HTMLMainElement final : public HTMLElement {
 public:
  explicit HTMLMainElement(Document&);
  void ChildrenChanged(const ChildrenChange&) override;
  InsertionNotificationRequest InsertedInto(ContainerNode&) override;

 private:
  void NotifySoftNavigationHeuristics();
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_MAIN_ELEMENT_H_
