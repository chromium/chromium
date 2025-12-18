// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_INSTALL_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_INSTALL_ELEMENT_H_

#include "third_party/blink/public/mojom/web_install/web_install.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/html_permission_element.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

namespace blink {

class Attribute;
class Document;
class KURL;
class String;

class CORE_EXPORT HTMLInstallElement : public HTMLPermissionElement {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit HTMLInstallElement(Document&);

  const String& InstallUrl() const;
  const String& ManifestId() const;
  void Trace(Visitor*) const override;

 private:
  // HTMLElement:
  bool IsURLAttribute(const Attribute&) const override;

  // HTMLPermissionElement:
  void UpdateAppearance() override;
  void DefaultEventHandler(Event&) override;

  void UpdateAppearanceTask();

  HeapMojoRemote<mojom::blink::WebInstallService>& WebInstallService();
  void OnConnectionError();

  void OnActivated();
  void OnInstallResult(mojom::blink::WebInstallServiceResult,
                       const KURL& manifest_id);

  HeapMojoRemote<mojom::blink::WebInstallService> service_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_INSTALL_ELEMENT_H_
