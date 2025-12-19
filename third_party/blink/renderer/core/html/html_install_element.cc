// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_install_element.h"

#include "third_party/blink/public/mojom/web_install/web_install.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/strings/grit/permission_element_strings.h"
#include "third_party/blink/renderer/core/dom/attribute.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/html/html_permission_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mojo/mojo_binding_context.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

HTMLInstallElement::HTMLInstallElement(Document& document)
    : HTMLPermissionElement(document, html_names::kInstallTag),
      service_(document.GetExecutionContext()) {
  CHECK(RuntimeEnabledFeatures::InstallElementEnabled(
      document.GetExecutionContext()));
  setType(AtomicString("install"));
}

const String& HTMLInstallElement::InstallUrl() const {
  return FastGetAttribute(html_names::kInstallurlAttr).GetString();
}

const String& HTMLInstallElement::ManifestId() const {
  return FastGetAttribute(html_names::kManifestidAttr).GetString();
}

void HTMLInstallElement::Trace(Visitor* visitor) const {
  visitor->Trace(service_);
  HTMLPermissionElement::Trace(visitor);
}

void HTMLInstallElement::UpdateAppearance() {
  // We'll update the text ourselves rather than invoking
  // `HTMLPermissionElement::UpdateAppearance()` given the logic here. Like
  // `HTMLGeolocationElement::UpdateAppearance()`, we'll punt the inner text
  // change out to a task on the default task queue.
  GetDocument()
      .GetTaskRunner(TaskType::kInternalDefault)
      ->PostTask(FROM_HERE, BindOnce(&HTMLInstallElement::UpdateAppearanceTask,
                                     WrapWeakPersistent(this)));
}

void HTMLInstallElement::UpdateAppearanceTask() {
  // TODO(crbug.com/467103133): Render site-specific information.
  uint16_t message_id = GetTranslatedMessageID(
      IDS_PERMISSION_REQUEST_INSTALL, ComputeInheritedLanguage().LowerASCII());
  String inner_text = GetLocale().QueryString(message_id);
  CHECK(message_id);
  permission_text_span()->setInnerText(inner_text);

  UpdateIcon(mojom::blink::PermissionName::WEB_APP_INSTALLATION);
}

bool HTMLInstallElement::IsURLAttribute(const Attribute& attr) const {
  return attr.GetName() == html_names::kManifestidAttr ||
         attr.GetName() == html_names::kInstallurlAttr ||
         HTMLElement::IsURLAttribute(attr);
}

void HTMLInstallElement::DefaultEventHandler(Event& event) {
  // We'll handle activation here, and punt everything else through
  // `HTMLPermissionElement`.
  if (event.type() == event_type_names::kDOMActivate) {
    HandleActivation(event, blink::BindOnce(&HTMLInstallElement::OnActivated,
                                            WrapWeakPersistent(this)));
    return;
  }
  HTMLPermissionElement::DefaultEventHandler(event);
}

HeapMojoRemote<mojom::blink::WebInstallService>&
HTMLInstallElement::WebInstallService() {
  if (!service_.is_bound()) {
    auto* context = GetDocument().GetExecutionContext();
    context->GetBrowserInterfaceBroker().GetInterface(
        service_.BindNewPipeAndPassReceiver(
            context->GetTaskRunner(TaskType::kMiscPlatformAPI)));
    // In case the other endpoint gets disconnected, we want to reset our end of
    // the pipe as well so that we don't remain connected to a half-open pipe.
    service_.set_disconnect_handler(BindOnce(
        &HTMLInstallElement::OnConnectionError, WrapWeakPersistent(this)));
  }
  return service_;
}

void HTMLInstallElement::OnConnectionError() {
  service_.reset();
}

void HTMLInstallElement::OnActivated() {
  // Do some installation.
  CHECK(WebInstallService());

  // Create the empty install data. If it remains empty, we attempt to install
  // the current page.
  mojom::blink::InstallOptionsPtr options;
  KURL install_url = KURL(InstallUrl());
  if (install_url.IsValid()) {
    options = mojom::blink::InstallOptions::New();
    options->install_url = install_url;
    // manifestid is only valid if installurl was also provided, as it's used
    // for data validation on the installurl.
    KURL manifest_id_url = KURL(ManifestId());
    if (manifest_id_url.IsValid()) {
      options->manifest_id = manifest_id_url;
    }
  }
  WebInstallService()->InstallFromElement(
      std::move(options),
      BindOnce(&HTMLInstallElement::OnInstallResult, WrapWeakPersistent(this)));
}

void HTMLInstallElement::OnInstallResult(
    mojom::blink::WebInstallServiceResult result,
    const KURL& manifest_id) {
  switch (result) {
    case mojom::blink::WebInstallServiceResult::kAbortError:
      DispatchEvent(
          *Event::CreateCancelableBubble(event_type_names::kPromptdismiss));
      break;
    case mojom::blink::WebInstallServiceResult::kDataError:
    case mojom::blink::WebInstallServiceResult::kSuccess:
      DispatchEvent(
          *Event::CreateCancelableBubble(event_type_names::kPromptaction));
      break;
  }
}

}  // namespace blink
