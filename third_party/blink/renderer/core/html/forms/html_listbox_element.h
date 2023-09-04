// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_HTML_LISTBOX_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_HTML_LISTBOX_ELEMENT_H_

#include "third_party/blink/renderer/core/html/html_element.h"

namespace blink {

class HTMLListboxElement : public HTMLElement {
 public:
  explicit HTMLListboxElement(Document&);

  InsertionNotificationRequest InsertedInto(ContainerNode&) override;
  void RemovedFrom(ContainerNode& insertion_point) override;

  static bool IsSelectlistAssociated(const Element*);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_HTML_LISTBOX_ELEMENT_H_
