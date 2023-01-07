// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PORTAL_HTML_PORTAL_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PORTAL_HTML_PORTAL_ELEMENT_H_

#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/portal/portal.mojom-blink-forward.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_or_worker_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class Document;
class PortalActivateOptions;
class PortalContents;
class PostMessageOptions;
class ScriptState;

// The HTMLPortalElement implements the <portal> HTML element. The portal
// element can be used to embed another top-level browsing context, which can be
// activated using script. The portal element is still under development and not
// part of the HTML standard. It can be enabled by passing
// --enable-features=Portals. See also https://github.com/WICG/portals.
class CORE_EXPORT HTMLPortalElement : public HTMLFrameOwnerElement {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // |portal_token|, |remote_portal| and |portal_client_receiver| are all empty
  // when an empty HTMLPortalElement is constructed, (it hasn't yet been
  // attached to an actual contents).
  explicit HTMLPortalElement(
      Document& document,
      const PortalToken* portal_token = nullptr,
      mojo::PendingAssociatedRemote<mojom::blink::Portal> remote_portal = {},
      mojo::PendingAssociatedReceiver<mojom::blink::PortalClient>
          portal_client_receiver = {});
  ~HTMLPortalElement() override;

  bool IsHTMLPortalElement() const final { return true; }

  // ScriptWrappable overrides.
  void Trace(Visitor* visitor) const override;

  // idl implementation.
  ScriptPromise activate(ScriptState*, PortalActivateOptions*, ExceptionState&);
  void postMessage(ScriptState* script_state,
                   const ScriptValue& message,
                   const PostMessageOptions* options,
                   ExceptionState& exception_state);
  EventListener* onmessage();
  void setOnmessage(EventListener* listener);
  EventListener* onmessageerror();
  void setOnmessageerror(EventListener* listener);

  const PortalToken& GetToken() const;

  FrameOwnerElementType OwnerType() const override {
    return FrameOwnerElementType::kPortal;
  }

  // Consumes the portal interface. When a Portal is activated, or if the
  // renderer receives a connection error, this function will gracefully
  // terminate the portal interface.
  void ConsumePortal();

  // Invoked when this element should no longer keep its guest contents alive
  // due to recent adoption.
  void ExpireAdoptionLifetime();

  // Called by PortalContents when it is about to be destroyed.
  void PortalContentsWillBeDestroyed(PortalContents*);

 private:
  // Returns a null string if the checks passed, and a suitable error otherwise.
  String PreActivateChecksCommon();

  // Performs a default activation (e.g. due to an unprevented click), as
  // opposed to one requested by invoking HTMLPortalElement::activate.
  void ActivateDefault();

  // Checks whether the Portals feature is enabled for this document, and logs a
  // warning to the developer if not. Doing basically anything with an
  // HTMLPortalElement in a document which doesn't support portals is forbidden.
  bool CheckPortalsEnabledOrWarn() const;
  bool CheckPortalsEnabledOrThrow(ExceptionState&) const;

  // Checks if, when inserted, we were beyond the frame limit. If so, we will
  // disable navigating the portal and insertion (and will display a warning in
  // the console).
  bool CheckWithinFrameLimitOrWarn() const;

  enum class GuestContentsEligibility {
    // Can have a guest contents.
    kEligible,

    // Ineligible as it is not top-level.
    kNotTopLevel,

    // Ineligible as it is sandboxed.
    kSandboxed,

    // Ineligible as the host's protocol is not in the HTTP family.
    kNotHTTPFamily,

    // Ineligible for additional reasons.
    kIneligible,
  };
  GuestContentsEligibility GetGuestContentsEligibility() const;
  bool CanHaveGuestContents() const {
    return GetGuestContentsEligibility() == GuestContentsEligibility::kEligible;
  }

  // Navigates the portal to |url_|.
  void Navigate();

  // Node overrides
  InsertionNotificationRequest InsertedInto(ContainerNode&) override;
  void RemovedFrom(ContainerNode&) override;
  void DefaultEventHandler(Event&) override;

  // Element overrides
  bool IsURLAttribute(const Attribute&) const override;
  void ParseAttribute(const AttributeModificationParams&) override;
  LayoutObject* CreateLayoutObject(const ComputedStyle&, LegacyLayout) override;
  bool SupportsFocus() const override;

  // HTMLFrameOwnerElement overrides
  void DisconnectContentFrame() override;
  ParsedPermissionsPolicy ConstructContainerPolicy() const override {
    return ParsedPermissionsPolicy();
  }
  void AttachLayoutTree(AttachContext& context) override;
  network::mojom::ReferrerPolicy ReferrerPolicyAttribute() override;

  bool IsPortalCreationOrAdoptionAllowed(const ContainerNode* node);

  // Defers the portal creation if the current document is being prerendered.
  void CreatePortalAndNavigate(const ContainerNode* node);

  Member<PortalContents> portal_;

  network::mojom::ReferrerPolicy referrer_policy_ =
      network::mojom::ReferrerPolicy::kDefault;

  // Temporarily set to keep this element alive after adoption.
  bool was_just_adopted_ = false;

  // Disable BackForwardCache when using the portal feature, because we do not
  // handle the state inside the portal after putting the page in cache.
  FrameOrWorkerScheduler::SchedulingAffectingFeatureHandle
      feature_handle_for_scheduler_;
};

// Type casting. Custom since adoption could lead to an HTMLPortalElement ending
// up in a document that doesn't have Portals enabled.
template <>
struct DowncastTraits<HTMLPortalElement> {
  static bool AllowFrom(const HTMLElement& element) {
    return element.IsHTMLPortalElement();
  }
  static bool AllowFrom(const Node& node) {
    if (const HTMLElement* html_element = DynamicTo<HTMLElement>(node))
      return html_element->IsHTMLPortalElement();
    return false;
  }
  static bool AllowFrom(const Element& element) {
    if (const HTMLElement* html_element = DynamicTo<HTMLElement>(element))
      return html_element->IsHTMLPortalElement();
    return false;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PORTAL_HTML_PORTAL_ELEMENT_H_
