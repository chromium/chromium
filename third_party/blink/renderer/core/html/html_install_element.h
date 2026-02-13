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
class KURL;
class String;

// Represents the <install> HTML element, which provides a mechanism to
// install web applications. It has two optional attributes:
// - installurl: URL of the web app to install. If not provided, the current
//   document URL is used.
// - manifestid: ID of the web app manifest. Only valid if installurl is also
//   provided.
// By default the element renders as an Install button, but may also show as
// a Launch button.
class CORE_EXPORT HTMLInstallElement : public HTMLCapabilityElementBase {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit HTMLInstallElement(Document&);

  const String& InstallUrl() const;
  const String& ManifestId() const;
  void Trace(Visitor*) const override;

  bool show_as_launch() const { return show_as_launch_; }

 private:
  // HTMLElement:
  bool IsURLAttribute(const Attribute&) const override;

  // HTMLCapabilityElementBase:
  void UpdateAppearance() override;
  void UpdateIcon(mojom::blink::PermissionName permission_name) override;
  mojom::blink::EmbeddedPermissionRequestDescriptorPtr
  CreateEmbeddedPermissionRequestDescriptor() override;
  void DefaultEventHandler(Event&) override;

  void OnIsInstalledResult(bool is_installed);
  void UpdateAppearanceTask(bool is_installed);

  // Returned remote is not guaranteed to be bound.
  HeapMojoRemote<mojom::blink::WebInstallService>& WebInstallService();
  void OnConnectionError();

  void OnActivated();
  mojom::blink::InstallOptionsPtr GetCheckedInstallOptions();
  void OnInstallResult(mojom::blink::WebInstallServiceResult,
                       const KURL& manifest_id);

  HeapMojoRemote<mojom::blink::WebInstallService> service_;
  // Controls whether the element should render as a launch button.
  bool show_as_launch_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_INSTALL_ELEMENT_H_
