/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 * Copyright (C) 2004, 2006, 2009, 2010 Apple Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_FRAME_SET_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_FRAME_SET_ELEMENT_H_

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/html/html_dimension.h"
#include "third_party/blink/renderer/core/html/html_element.h"

namespace blink {

class HTMLFrameSetElement final : public HTMLElement {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit HTMLFrameSetElement(Document&);

  bool HasFrameBorder() const { return frameborder_; }
  bool NoResize() const { return noresize_; }

  wtf_size_t TotalRows() const {
    return std::max<wtf_size_t>(1, row_lengths_.size());
  }
  wtf_size_t TotalCols() const {
    return std::max<wtf_size_t>(1, col_lengths_.size());
  }
  int Border() const { return HasFrameBorder() ? border_ : 0; }

  bool HasBorderColor() const { return border_color_set_; }

  const Vector<HTMLDimension>& RowLengths() const { return row_lengths_; }
  const Vector<HTMLDimension>& ColLengths() const { return col_lengths_; }

  bool HasNonInBodyInsertionMode() const override { return true; }

  DEFINE_WINDOW_ATTRIBUTE_EVENT_LISTENER(blur, kBlur)
  DEFINE_WINDOW_ATTRIBUTE_EVENT_LISTENER(error, kError)
  DEFINE_WINDOW_ATTRIBUTE_EVENT_LISTENER(focus, kFocus)
  DEFINE_WINDOW_ATTRIBUTE_EVENT_LISTENER(load, kLoad)
  DEFINE_WINDOW_ATTRIBUTE_EVENT_LISTENER(resize, kResize)
  DEFINE_WINDOW_ATTRIBUTE_EVENT_LISTENER(scroll, kScroll)
  DEFINE_WINDOW_ATTRIBUTE_EVENT_LISTENER(orientationchange, kOrientationchange)

 private:
  void ParseAttribute(const AttributeModificationParams&) override;
  bool IsPresentationAttribute(const QualifiedName&) const override;
  void CollectStyleForPresentationAttribute(
      const QualifiedName&,
      const AtomicString&,
      MutableCSSPropertyValueSet*) override;

  void AttachLayoutTree(AttachContext&) override;
  bool LayoutObjectIsNeeded(const ComputedStyle&) const override;
  LayoutObject* CreateLayoutObject(const ComputedStyle&, LegacyLayout) override;

  void DefaultEventHandler(Event&) override;

  InsertionNotificationRequest InsertedInto(ContainerNode&) override;
  void WillRecalcStyle(const StyleRecalcChange) override;

  Vector<HTMLDimension> row_lengths_;
  Vector<HTMLDimension> col_lengths_;

  int border_;
  bool border_set_;

  bool border_color_set_;

  bool frameborder_;
  bool frameborder_set_;
  bool noresize_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_FRAME_SET_ELEMENT_H_
