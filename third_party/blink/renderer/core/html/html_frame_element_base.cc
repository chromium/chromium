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

#include "services/network/public/cpp/web_sandbox_flags.h"
#include "services/network/public/mojom/web_sandbox_flags.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/binding_security.h"
#include "third_party/blink/renderer/bindings/core/v8/js_event_handler_for_content_attribute.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/attribute.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/frame_console.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/remote_frame.h"
#include "third_party/blink/renderer/core/frame/remote_frame_view.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

HTMLFrameElementBase::HTMLFrameElementBase(const QualifiedName& tag_name,
                                           Document& document)
    : HTMLFrameOwnerElement(tag_name, document),
      scrollbar_mode_(mojom::blink::ScrollbarMode::kAuto),
      margin_width_(-1),
      margin_height_(-1) {}

void HTMLFrameElementBase::OpenURL(bool replace_current_item) {
  LocalFrame* parent_frame = GetDocument().GetFrame();
  if (!parent_frame) {
    return;
  }

  if (url_.empty())
    url_ = AtomicString(BlankURL().GetString());
  KURL url = GetDocument().CompleteURL(url_);
  if (ContentFrame() && !parent_frame->CanNavigate(*ContentFrame(), url)) {
    return;
  }

  // There is no (easy) way to tell if |url_| is relative at this point. That
  // is determined in the KURL constructor. If we fail to create an absolute
  // URL at this point, *and* the base URL is a data URL, assume |url_| was
  // relative and give a warning.
  if (!url.IsValid() && GetDocument().BaseURL().ProtocolIsData()) {
    GetExecutionContext()->AddConsoleMessage(
        MakeGarbageCollected<ConsoleMessage>(
            mojom::ConsoleMessageSource::kRendering,
            mojom::ConsoleMessageLevel::kWarning,
            "Invalid relative frame source URL (" + url_ +
                ") within data URL."));
  }
  LoadOrRedirectSubframe(url, frame_name_, replace_current_item);
}

void HTMLFrameElementBase::ParseAttribute(
    const AttributeModificationParams& params) {
  const QualifiedName& name = params.name;
  const AtomicString& value = params.new_value;
  if (name == html_names::kSrcdocAttr) {
    String srcdoc_value = "";
    if (!value.IsNull())
      srcdoc_value = FastGetAttribute(html_names::kSrcdocAttr).GetString();
    if (ContentFrame()) {
      GetDocument().GetFrame()->GetLocalFrameHostRemote().DidChangeSrcDoc(
          ContentFrame()->GetFrameToken(), srcdoc_value);
    }
    if (!value.IsNull()) {
      SetLocation(SrcdocURL().GetString());
    } else {
      const AtomicString& src_value = FastGetAttribute(html_names::kSrcAttr);
      if (!src_value.IsNull()) {
        SetLocation(StripLeadingAndTrailingHTMLSpaces(src_value));
      } else if (!params.old_value.IsNull()) {
        // We're resetting kSrcdocAttr, but kSrcAttr has no value, so load
        // about:blank. https://crbug.com/1233143
        SetLocation(BlankURL());
      }
    }
  } else if (name == html_names::kSrcAttr &&
             !FastHasAttribute(html_names::kSrcdocAttr)) {
    SetLocation(StripLeadingAndTrailingHTMLSpaces(value));
  } else if (name == html_names::kIdAttr) {
    // Important to call through to base for the id attribute so the hasID bit
    // gets set.
    HTMLFrameOwnerElement::ParseAttribute(params);
    frame_name_ = value;
  } else if (name == html_names::kNameAttr) {
    frame_name_ = value;
  } else if (name == html_names::kMarginwidthAttr) {
    SetMarginWidth(value.ToInt());
  } else if (name == html_names::kMarginheightAttr) {
    SetMarginHeight(value.ToInt());
  } else if (name == html_names::kScrollingAttr) {
    // https://html.spec.whatwg.org/multipage/rendering.html#the-page:
    // If [the scrolling] attribute's value is an ASCII
    // case-insensitive match for the string "off", "noscroll", or "no", then
    // the user agent is expected to prevent any scrollbars from being shown for
    // the viewport of the Document's browsing context, regardless of the
    // 'overflow' property that applies to that viewport.
    if (EqualIgnoringASCIICase(value, "off") ||
        EqualIgnoringASCIICase(value, "noscroll") ||
        EqualIgnoringASCIICase(value, "no"))
      SetScrollbarMode(mojom::blink::ScrollbarMode::kAlwaysOff);
    else
      SetScrollbarMode(mojom::blink::ScrollbarMode::kAuto);
  } else if (name == html_names::kOnbeforeunloadAttr) {
    // FIXME: should <frame> elements have beforeunload handlers?
    SetAttributeEventListener(
        event_type_names::kBeforeunload,
        JSEventHandlerForContentAttribute::Create(
            GetExecutionContext(), name, value,
            JSEventHandler::HandlerType::kOnBeforeUnloadEventHandler));
  } else {
    HTMLFrameOwnerElement::ParseAttribute(params);
  }
}

scoped_refptr<const SecurityOrigin>
HTMLFrameElementBase::GetOriginForPermissionsPolicy() const {
  // Sandboxed frames have a unique origin.
  if ((GetFramePolicy().sandbox_flags &
       network::mojom::blink::WebSandboxFlags::kOrigin) !=
      network::mojom::blink::WebSandboxFlags::kNone) {
    return SecurityOrigin::CreateUniqueOpaque();
  }

  // If the frame will inherit its origin from the owner, then use the owner's
  // origin when constructing the container policy.
  KURL url = GetDocument().CompleteURL(url_);
  if (Document::ShouldInheritSecurityOriginFromOwner(url))
    return GetExecutionContext()->GetSecurityOrigin();

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
  // Except for when state-preserving atomic moves are enabled, we should never
  // have a content frame at the point where we got inserted into a tree.
  SECURITY_CHECK(!ContentFrame() ||
                 GetDocument().StatePreservingAtomicMoveInProgress());
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

int HTMLFrameElementBase::DefaultTabIndex() const {
  // The logic in focus_controller.cc requires frames to return
  // true for IsFocusable(). However, frames are not actually
  // focusable, and focus_controller.cc takes care of moving
  // focus within the frame focus scope.
  // TODO(crbug.com/1444450) It would be better to remove this
  // override entirely, and make SupportsFocus() return false.
  // That would require adding logic in focus_controller.cc that
  // ignores IsFocusable for HTMLFrameElementBase. At that point,
  // AXObject::IsKeyboardFocusable() can also have special case
  // code removed.
  return 0;
}

void HTMLFrameElementBase::SetFocused(bool received,
                                      mojom::blink::FocusType focus_type) {
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
  return attribute.GetName() == html_names::kLongdescAttr ||
         attribute.GetName() == html_names::kSrcAttr ||
         HTMLFrameOwnerElement::IsURLAttribute(attribute);
}

bool HTMLFrameElementBase::HasLegalLinkAttribute(
    const QualifiedName& name) const {
  return name == html_names::kSrcAttr ||
         HTMLFrameOwnerElement::HasLegalLinkAttribute(name);
}

bool HTMLFrameElementBase::IsHTMLContentAttribute(
    const Attribute& attribute) const {
  return attribute.GetName() == html_names::kSrcdocAttr ||
         HTMLFrameOwnerElement::IsHTMLContentAttribute(attribute);
}

void HTMLFrameElementBase::SetScrollbarMode(
    mojom::blink::ScrollbarMode scrollbar_mode) {
  if (scrollbar_mode_ == scrollbar_mode)
    return;

  if (contentDocument()) {
    contentDocument()->WillChangeFrameOwnerProperties(
        margin_width_, margin_height_, scrollbar_mode, IsDisplayNone(),
        GetColorScheme(), GetPreferredColorScheme());
  }
  scrollbar_mode_ = scrollbar_mode;
  FrameOwnerPropertiesChanged();
}

void HTMLFrameElementBase::SetMarginWidth(int margin_width) {
  if (margin_width_ == margin_width)
    return;

  if (contentDocument()) {
    contentDocument()->WillChangeFrameOwnerProperties(
        margin_width, margin_height_, scrollbar_mode_, IsDisplayNone(),
        GetColorScheme(), GetPreferredColorScheme());
  }
  margin_width_ = margin_width;
  FrameOwnerPropertiesChanged();
}

void HTMLFrameElementBase::SetMarginHeight(int margin_height) {
  if (margin_height_ == margin_height)
    return;

  if (contentDocument()) {
    contentDocument()->WillChangeFrameOwnerProperties(
        margin_width_, margin_height, scrollbar_mode_, IsDisplayNone(),
        GetColorScheme(), GetPreferredColorScheme());
  }
  margin_height_ = margin_height;
  FrameOwnerPropertiesChanged();
}

}  // namespace blink
