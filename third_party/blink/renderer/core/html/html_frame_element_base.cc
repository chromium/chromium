/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann (hausmann@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
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
 */

#include "third_party/blink/renderer/core/html/html_frame_element_base.h"

#include "third_party/blink/renderer/bindings/core/v8/binding_security.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/script_event_listener.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/attribute.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/remote_frame.h"
#include "third_party/blink/renderer/core/frame/remote_frame_view.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"

namespace blink {

using namespace HTMLNames;

HTMLFrameElementBase::HTMLFrameElementBase(const QualifiedName& tag_name,
                                           Document& document)
    : HTMLFrameOwnerElement(tag_name, document),
      scrolling_mode_(kScrollbarAuto),
      margin_width_(-1),
      margin_height_(-1) {}

bool HTMLFrameElementBase::IsURLAllowed() const {
  if (url_.IsEmpty())
    return true;

  const KURL& complete_url = GetDocument().CompleteURL(url_);

  if (ContentFrame() && complete_url.ProtocolIsJavaScript()) {
    // Check if the caller can execute script in the context of the content
    // frame. NB: This check can be invoked without any JS on the stack for some
    // parser operations. In such case, we use the origin of the frame element's
    // containing document as the caller context.
    v8::Isolate* isolate = ToIsolate(&GetDocument());
    LocalDOMWindow* accessing_window = isolate->InContext()
                                           ? CurrentDOMWindow(isolate)
                                           : GetDocument().domWindow();
    if (!BindingSecurity::ShouldAllowAccessToFrame(
            accessing_window, ContentFrame(),
            BindingSecurity::ErrorReportOption::kReport))
      return false;
  }
  return true;
}

void HTMLFrameElementBase::OpenURL(bool replace_current_item) {
  if (!IsURLAllowed())
    return;

  if (url_.IsEmpty())
    url_ = AtomicString(BlankURL().GetString());

  LocalFrame* parent_frame = GetDocument().GetFrame();
  if (!parent_frame)
    return;

  // Support for <frame src="javascript:string">
  KURL script_url;
  KURL url = GetDocument().CompleteURL(url_);
  if (url.ProtocolIsJavaScript()) {
    // We'll set/execute |scriptURL| iff CSP allows us to execute inline
    // JavaScript. If CSP blocks inline JavaScript, then exit early if
    // we're trying to execute script in an existing document. If we're
    // executing JavaScript to create a new document (e.g.
    // '<iframe src="javascript:...">' then continue loading 'about:blank'
    // so that the frame is populated with something reasonable.
    if (ContentSecurityPolicy::ShouldBypassMainWorld(&GetDocument()) ||
        GetDocument().GetContentSecurityPolicy()->AllowJavaScriptURLs(
            this, url.GetString(), GetDocument().Url(),
            OrdinalNumber::First())) {
      script_url = url;
    } else {
      if (ContentFrame())
        return;
    }

    url = BlankURL();
  }

  if (!LoadOrRedirectSubframe(url, frame_name_, replace_current_item))
    return;
  if (!ContentFrame() || script_url.IsEmpty() ||
      !ContentFrame()->IsLocalFrame())
    return;
  if (ContentFrame()->Owner()->GetSandboxFlags() & kSandboxOrigin)
    return;
  ToLocalFrame(ContentFrame())
      ->GetScriptController()
      .ExecuteScriptIfJavaScriptURL(script_url, this);
}

void HTMLFrameElementBase::ParseAttribute(
    const AttributeModificationParams& params) {
  const QualifiedName& name = params.name;
  const AtomicString& value = params.new_value;
  if (name == srcdocAttr) {
    if (!value.IsNull()) {
      SetLocation(SrcdocURL().GetString());
    } else {
      const AtomicString& src_value = FastGetAttribute(srcAttr);
      if (!src_value.IsNull())
        SetLocation(StripLeadingAndTrailingHTMLSpaces(src_value));
    }
  } else if (name == srcAttr && !FastHasAttribute(srcdocAttr)) {
    SetLocation(StripLeadingAndTrailingHTMLSpaces(value));
  } else if (name == idAttr) {
    // Important to call through to base for the id attribute so the hasID bit
    // gets set.
    HTMLFrameOwnerElement::ParseAttribute(params);
    frame_name_ = value;
  } else if (name == nameAttr) {
    frame_name_ = value;
  } else if (name == marginwidthAttr) {
    SetMarginWidth(value.ToInt());
  } else if (name == marginheightAttr) {
    SetMarginHeight(value.ToInt());
  } else if (name == scrollingAttr) {
    // Auto and yes both simply mean "allow scrolling." No means "don't allow
    // scrolling."
    if (DeprecatedEqualIgnoringCase(value, "auto") ||
        DeprecatedEqualIgnoringCase(value, "yes"))
      SetScrollingMode(kScrollbarAuto);
    else if (DeprecatedEqualIgnoringCase(value, "no"))
      SetScrollingMode(kScrollbarAlwaysOff);
  } else if (name == onbeforeunloadAttr) {
    // FIXME: should <frame> elements have beforeunload handlers?
    SetAttributeEventListener(
        EventTypeNames::beforeunload,
        CreateAttributeEventListener(
            this, name, value,
            JSEventHandler::HandlerType::kOnBeforeUnloadEventHandler));
  } else {
    HTMLFrameOwnerElement::ParseAttribute(params);
  }
}

scoped_refptr<const SecurityOrigin>
HTMLFrameElementBase::GetOriginForFeaturePolicy() const {
  // Sandboxed frames have a unique origin.
  if (GetSandboxFlags() & kSandboxOrigin)
    return SecurityOrigin::CreateUniqueOpaque();

  // If the frame will inherit its origin from the owner, then use the owner's
  // origin when constructing the container policy.
  KURL url = GetDocument().CompleteURL(url_);
  if (Document::ShouldInheritSecurityOriginFromOwner(url))
    return GetDocument().GetSecurityOrigin();

  // Other frames should use the origin defined by the absolute URL (this will
  // be a unique origin for data: URLs)
  return SecurityOrigin::Create(url);
}

void HTMLFrameElementBase::SetNameAndOpenURL() {
  frame_name_ = GetNameAttribute();
  OpenURL();
}

Node::InsertionNotificationRequest HTMLFrameElementBase::InsertedInto(
    ContainerNode& insertion_point) {
  HTMLFrameOwnerElement::InsertedInto(insertion_point);
  // We should never have a content frame at the point where we got inserted
  // into a tree.
  SECURITY_CHECK(!ContentFrame());
  return kInsertionShouldCallDidNotifySubtreeInsertions;
}

void HTMLFrameElementBase::DidNotifySubtreeInsertionsToDocument() {
  if (!GetDocument().GetFrame())
    return;

  if (!SubframeLoadingDisabler::CanLoadFrame(*this))
    return;

  // It's possible that we already have ContentFrame(). Arbitrary user code can
  // run between InsertedInto() and DidNotifySubtreeInsertionsToDocument().
  if (!ContentFrame())
    SetNameAndOpenURL();
}

void HTMLFrameElementBase::AttachLayoutTree(AttachContext& context) {
  HTMLFrameOwnerElement::AttachLayoutTree(context);

  if (GetLayoutEmbeddedContent() && ContentFrame())
    SetEmbeddedContentView(ContentFrame()->View());
}

void HTMLFrameElementBase::SetLocation(const String& str) {
  url_ = AtomicString(str);

  if (isConnected())
    OpenURL(false);
}

bool HTMLFrameElementBase::SupportsFocus() const {
  return true;
}

void HTMLFrameElementBase::SetFocused(bool received, WebFocusType focus_type) {
  HTMLFrameOwnerElement::SetFocused(received, focus_type);
  if (Page* page = GetDocument().GetPage()) {
    if (received) {
      page->GetFocusController().SetFocusedFrame(ContentFrame());
    } else if (page->GetFocusController().FocusedFrame() == ContentFrame()) {
      // Focus may have already been given to another frame, don't take it away.
      page->GetFocusController().SetFocusedFrame(nullptr);
    }
  }
}

bool HTMLFrameElementBase::IsURLAttribute(const Attribute& attribute) const {
  return attribute.GetName() == longdescAttr ||
         attribute.GetName() == srcAttr ||
         HTMLFrameOwnerElement::IsURLAttribute(attribute);
}

bool HTMLFrameElementBase::HasLegalLinkAttribute(
    const QualifiedName& name) const {
  return name == srcAttr || HTMLFrameOwnerElement::HasLegalLinkAttribute(name);
}

bool HTMLFrameElementBase::IsHTMLContentAttribute(
    const Attribute& attribute) const {
  return attribute.GetName() == srcdocAttr ||
         HTMLFrameOwnerElement::IsHTMLContentAttribute(attribute);
}

void HTMLFrameElementBase::SetScrollingMode(ScrollbarMode scrollbar_mode) {
  if (scrolling_mode_ == scrollbar_mode)
    return;

  if (contentDocument()) {
    contentDocument()->WillChangeFrameOwnerProperties(
        margin_width_, margin_height_, scrollbar_mode, IsDisplayNone());
  }
  scrolling_mode_ = scrollbar_mode;
  FrameOwnerPropertiesChanged();
}

void HTMLFrameElementBase::SetMarginWidth(int margin_width) {
  if (margin_width_ == margin_width)
    return;

  if (contentDocument()) {
    contentDocument()->WillChangeFrameOwnerProperties(
        margin_width, margin_height_, scrolling_mode_, IsDisplayNone());
  }
  margin_width_ = margin_width;
  FrameOwnerPropertiesChanged();
}

void HTMLFrameElementBase::SetMarginHeight(int margin_height) {
  if (margin_height_ == margin_height)
    return;

  if (contentDocument()) {
    contentDocument()->WillChangeFrameOwnerProperties(
        margin_width_, margin_height, scrolling_mode_, IsDisplayNone());
  }
  margin_height_ = margin_height;
  FrameOwnerPropertiesChanged();
}

}  // namespace blink
