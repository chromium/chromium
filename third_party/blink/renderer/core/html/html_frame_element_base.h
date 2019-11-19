/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann <hausmann@kde.org>
 * Copyright (C) 2004, 2006, 2008, 2009 Apple Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_FRAME_ELEMENT_BASE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_FRAME_ELEMENT_BASE_H_

#include "third_party/blink/public/platform/web_focus_type.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"

namespace blink {

class CORE_EXPORT HTMLFrameElementBase : public HTMLFrameOwnerElement {
 public:
  bool CanContainRangeEndPoint() const final { return false; }

  // FrameOwner overrides:
  ScrollbarMode ScrollingMode() const final { return scrolling_mode_; }
  int MarginWidth() const final { return margin_width_; }
  int MarginHeight() const final { return margin_height_; }

 protected:
  friend class HTMLFrameElementTest;
  friend class HTMLIFrameElementTest;

  HTMLFrameElementBase(const QualifiedName&, Document&);

  void ParseAttribute(const AttributeModificationParams&) override;

  InsertionNotificationRequest InsertedInto(ContainerNode&) override;
  void DidNotifySubtreeInsertionsToDocument() final;
  void AttachLayoutTree(AttachContext&) override;

  void SetScrollingMode(ScrollbarMode);
  void SetMarginWidth(int);
  void SetMarginHeight(int);

  // Return the origin which is to be used for feature policy container
  // policies, when the "allow" attribute is used. When that attribute is used,
  // the feature policy which is constructed should only allow a given feature
  // on the origin which is specified by the frame's "src" attribute.
  // It also takes into account details such as the frame's sandbox status, and
  // whether the frame should inherit its parent's origin.
  scoped_refptr<const SecurityOrigin> GetOriginForFeaturePolicy()
      const override;

 private:
  bool SupportsFocus() const final;
  int DefaultTabIndex() const final;
  void SetFocused(bool, WebFocusType) final;

  bool IsURLAttribute(const Attribute&) const final;
  bool HasLegalLinkAttribute(const QualifiedName&) const final;
  bool IsHTMLContentAttribute(const Attribute&) const final;

  bool AreAuthorShadowsAllowed() const final { return false; }

  void SetLocation(const String&);
  void SetNameAndOpenURL();
  bool IsURLAllowed() const;
  void OpenURL(bool replace_current_item = true);

  ScrollbarMode scrolling_mode_;
  int margin_width_;
  int margin_height_;

  AtomicString url_;
  AtomicString frame_name_;
};

inline bool IsHTMLFrameElementBase(const HTMLElement& element) {
  return IsA<HTMLFrameElement>(element) || IsA<HTMLIFrameElement>(element);
}

DEFINE_HTMLELEMENT_TYPE_CASTS_WITH_FUNCTION(HTMLFrameElementBase);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_FRAME_ELEMENT_BASE_H_
