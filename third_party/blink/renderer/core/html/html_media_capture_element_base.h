// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_MEDIA_CAPTURE_ELEMENT_BASE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_MEDIA_CAPTURE_ELEMENT_BASE_H_

#include "base/time/time.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom-blink-forward.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/html_capability_element_base.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class DOMException;

class CORE_EXPORT HTMLMediaCaptureElementBase
    : public HTMLCapabilityElementBase,
      public Supplementable<HTMLMediaCaptureElementBase> {
 public:
  HTMLMediaCaptureElementBase(Document& document,
                              const QualifiedName& tag_name);
  void Trace(Visitor*) const override;

  DOMException* error() const { return error_.Get(); }
  void SetError(DOMException* error) { error_ = error; }

  DEFINE_ATTRIBUTE_EVENT_LISTENER(cancel, kCancel)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(error, kError)

  // HTML Element
  bool IsHTMLMediaCaptureElementBase() const final { return true; }

  // HTMLCapabilityElementBase
  void OnPermissionStatusChange(mojom::blink::PermissionName permission_name,
                                mojom::blink::PermissionStatus status) override;
  void OnEmbeddedPermissionsDecided(
      mojom::blink::EmbeddedPermissionControlResult result) override;

  Node::InsertionNotificationRequest InsertedInto(ContainerNode&) override;

  void DefaultEventHandler(Event& event) override;
  mojom::blink::EmbeddedPermissionRequestDescriptorPtr
  CreateEmbeddedPermissionRequestDescriptor() override;
  void OnActivationFailed(const String& error_message) override;

  virtual void ApplyDefaultConstraints();

  void ResetMediaStreamRequestTime();

  void UpdateAppearance() override;
  void UpdateIcon(mojom::blink::PermissionName permission) override;

  static mojom::blink::PermissionDescriptorPtr CreatePermissionDescriptor(
      mojom::blink::PermissionName name);

  const Vector<mojom::blink::PermissionDescriptorPtr>&
  GetPermissionDescriptors() const {
    return permission_descriptors_;
  }

 protected:
  static uint16_t GetUntranslatedMessageIDSinglePermission(
      mojom::blink::PermissionName name,
      bool granted);
  static uint16_t GetUntranslatedMessageIDMultiplePermissions(bool granted);

  virtual bool ShouldShowGrantedAppearance() const { return false; }

 private:
  void StartMediaStreamRequest();

  base::TimeTicks media_stream_request_start_time_;
  Member<DOMException> error_;
};

// Custom type casting traits for the base class so
// IsA<HTMLMediaCaptureElementBase> and DynamicTo<HTMLMediaCaptureElementBase>
// can be used on elements deriving from HTMLMediaCaptureElementBase.
template <>
struct DowncastTraits<HTMLMediaCaptureElementBase> {
  static bool AllowFrom(const HTMLElement& element) {
    return element.IsHTMLMediaCaptureElementBase();
  }
  static bool AllowFrom(const Node& node) {
    if (const HTMLElement* html_element = DynamicTo<HTMLElement>(node)) {
      return html_element->IsHTMLMediaCaptureElementBase();
    }
    return false;
  }
  static bool AllowFrom(const Element& element) {
    if (const HTMLElement* html_element = DynamicTo<HTMLElement>(element)) {
      return html_element->IsHTMLMediaCaptureElementBase();
    }
    return false;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_MEDIA_CAPTURE_ELEMENT_BASE_H_
