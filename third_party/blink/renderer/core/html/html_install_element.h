// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_INSTALL_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_INSTALL_ELEMENT_H_

#include "third_party/blink/public/mojom/web_install/web_install.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/html_capability_element_base.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

namespace blink {

class Attribute;
class Document;
class String;

// Represents the <install> HTML element, which provides a mechanism to
// install web applications. It has two optional attributes:
// - manifest: URL of the web app manifest to install.
// - manifestid: ID of the web app manifest.
// By default the element renders as an Install button, but may also show as
// a Launch button.
class CORE_EXPORT HTMLInstallElement : public HTMLCapabilityElementBase {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // Result of an install triggered by this element, surfaced to script via the
  // `installresult` event's `result` attribute.
  enum class InstallResult {
    kSuccess,     // The app was installed.
    kAbortError,  // The user or user agent cancelled the install.
    kDataError,   // The supplied install data (URL/manifest/id) was invalid.
  };

  explicit HTMLInstallElement(Document&);

  ElementType GetElementType() const final {
    return ElementType::kHTMLInstallElement;
  }

  // HTMLElement:
  bool IsHTMLInstallElement() const final { return true; }

  const String& ManifestId() const;
  const String& Manifest() const;

  DEFINE_ATTRIBUTE_EVENT_LISTENER(installresult, kInstallresult)

  void Trace(Visitor*) const override;

  bool show_as_launch() const { return show_as_launch_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(HTMLInstallElementTestBase,
                           InstalledStateHiddenInCanvasSubtree);
  FRIEND_TEST_ALL_PREFIXES(HTMLInstallElementTestBase,
                           InstalledStateClearedWhenMovedIntoCanvasSubtree);

  // HTMLElement:
  bool IsURLAttribute(const Attribute&) const override;
  void DidChangeIsInCanvasSubtree() override;

  // HTMLCapabilityElementBase:
  void UpdateAppearance() override;
  void UpdateIcon(mojom::blink::PermissionName permission) override;
  mojom::blink::EmbeddedPermissionRequestDescriptorPtr
  CreateEmbeddedPermissionRequestDescriptor() override;
  void DefaultEventHandler(Event&) override;

  void OnIsInstalledResult(bool is_installed);
  void UpdateAppearanceTask(bool is_installed);

  // Returned remote is not guaranteed to be bound.
  HeapMojoRemote<mojom::blink::WebInstallService>& WebInstallService();
  void OnConnectionError();

  void OnActivated();
  mojom::blink::ManifestInstallOptionsPtr GetCheckedManifestInstallOptions();
  void OnManifestInstallResult(mojom::blink::WebInstallServiceResult);

  // Enqueues a bubbling `installresult` event carrying `result` for
  // asynchronous dispatch.
  void DispatchInstallResultEvent(InstallResult result);

  HeapMojoRemote<mojom::blink::WebInstallService> service_;
  // Controls whether the element should render as a launch button.
  bool show_as_launch_ = false;
};
// The custom type casting is required for the InstallElement OT because the
// generated helpers code can lead to a compilation error or an
// HTMLInstallElement appearing in a document that does not have the
// InstallElement origin trial enabled (this would result in the creation of
// an HTMLUnknownElement with the "install" tag name).
// See
// https://chromium.googlesource.com/chromium/src.git/+/main/docs/custom_type_helpers_for_origin_trial_elements.md
// for more details.
template <>
struct DowncastTraits<HTMLInstallElement> {
  static bool AllowFrom(const HTMLElement& element) {
    return element.IsHTMLInstallElement();
  }
  static bool AllowFrom(const Node& node) {
    if (const HTMLElement* html_element = DynamicTo<HTMLElement>(node)) {
      return html_element->IsHTMLInstallElement();
    }
    return false;
  }
  static bool AllowFrom(const Element& element) {
    if (const HTMLElement* html_element = DynamicTo<HTMLElement>(element)) {
      return html_element->IsHTMLInstallElement();
    }
    return false;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_INSTALL_ELEMENT_H_
