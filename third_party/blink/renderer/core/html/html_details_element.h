/*
 * Copyright (C) 2010, 2011 Nokia Corporation and/or its subsidiary(-ies)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_DETAILS_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_DETAILS_ELEMENT_H_

#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cancellable_task.h"

namespace blink {

class HTMLDetailsElement final : public HTMLElement {
  DEFINE_WRAPPERTYPEINFO();

 public:
  void ToggleOpen();

  explicit HTMLDetailsElement(Document&);
  ~HTMLDetailsElement() override;

  Element* FindMainSummary() const;

  // Used for slot assignment.
  static bool IsFirstSummary(const Node&);

 private:
  void DispatchPendingEvent(const AttributeModificationReason);

  LayoutObject* CreateLayoutObject(const ComputedStyle&, LegacyLayout) override;
  void ParseAttribute(const AttributeModificationParams&) override;
  void DidAddUserAgentShadowRoot(ShadowRoot&) override;
  bool IsInteractiveContent() const override;

  bool is_open_;
  TaskHandle pending_event_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_DETAILS_ELEMENT_H_
