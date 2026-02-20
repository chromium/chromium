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
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/html_capability_element_base.h"
#include "third_party/blink/renderer/core/html/html_permission_icon_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mojo/mojo_binding_context.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

HTMLInstallElement::HTMLInstallElement(Document& document)
    : HTMLCapabilityElementBase(document, html_names::kInstallTag),
      service_(document.GetExecutionContext()) {
  CHECK(RuntimeEnabledFeatures::InstallElementEnabled(
      document.GetExecutionContext()));
  type_ = AtomicString("install");
  auto descriptor = mojom::blink::PermissionDescriptor::New();
  descriptor->name = mojom::blink::PermissionName::WEB_APP_INSTALLATION;
  permission_descriptors_.push_back(std::move(descriptor));
  UseCounter::CountWebDXFeature(document, WebDXFeature::kDRAFT_InstallElement);
}

const String& HTMLInstallElement::InstallUrl() const {
  return FastGetAttribute(html_names::kInstallurlAttr).GetString();
}

const String& HTMLInstallElement::ManifestId() const {
  return FastGetAttribute(html_names::kManifestidAttr).GetString();
}

void HTMLInstallElement::Trace(Visitor* visitor) const {
  visitor->Trace(service_);
  HTMLCapabilityElementBase::Trace(visitor);
}

void HTMLInstallElement::UpdateAppearance() {
  if (!WebInstallService().is_bound()) {
    // Do nothing if the document's execution context is gone.
    return;
  }

  // If no attributes provided, check if current document is already installed.
  if (InstallUrl().empty() && ManifestId().empty()) {
    // TODO(crbug.com/485281836): For now, always return false while we discuss
    // the appropriate long-term mitigation for width-based side channel
    // attacks. ("Launch" is slightly wider than "Install").
    OnIsInstalledResult(false);
    return;
  }

  // TODO(crbug.com/477643920): Evaluate element behavior with illegal/invalid
  // attributes. (Should we hide or grey out button, etc.).
  mojom::blink::InstallOptionsPtr options = GetCheckedInstallOptions();

  if (!options) {
    // Illegal arguments will never be installed. Skip straight to the
    // IsInstalled result so we can post the UpdateAppearanceTask.
    OnIsInstalledResult(false);
    return;
  }

  // Query installation status to update button text ("Install" vs "Launch").
  // TODO(crbug.com/485281836): For now, always return false while we discuss
  // the appropriate long-term mitigation for width-based side channel attacks.
  // ("Launch" is slightly wider than "Install").
  OnIsInstalledResult(false);
}

mojom::blink::EmbeddedPermissionRequestDescriptorPtr
HTMLInstallElement::CreateEmbeddedPermissionRequestDescriptor() {
  auto descriptor = mojom::blink::EmbeddedPermissionRequestDescriptor::New();
  descriptor->element_position = BoundsInWidget();

  auto install_descriptor =
      mojom::blink::InstallEmbeddedPermissionRequestDescriptor::New();
  descriptor->detail =
      mojom::blink::EmbeddedPermissionControlDescriptorExtension::NewInstall(
          std::move(install_descriptor));

  return descriptor;
}

void HTMLInstallElement::OnIsInstalledResult(bool is_installed) {
  // If this element points to an app that is already installed in the browser
  // process, the element will present itself as a launch button.
  show_as_launch_ = is_installed;

  // This is posted as a task, as similar code in
  // `HTMLGeolocationElement::UpdateAppearance` would crash due to DCHECKs being
  // hit for calling setInnerText during layout.
  // TODO(crbug.com/477974745): If possible, bind the mojo pipe to a task runner
  // that cannot be called during layout, to avoid this and simplify
  // <geolocation> too.
  GetDocument()
      .GetTaskRunner(TaskType::kInternalDefault)
      ->PostTask(FROM_HERE, BindOnce(&HTMLInstallElement::UpdateAppearanceTask,
                                     WrapWeakPersistent(this), is_installed));
}

void HTMLInstallElement::UpdateAppearanceTask(bool is_installed) {
  // TODO(crbug.com/467103133): Render site-specific information.
  uint16_t message_id =
      GetTranslatedMessageID(is_installed ? IDS_PERMISSION_REQUEST_LAUNCH
                                          : IDS_PERMISSION_REQUEST_INSTALL,
                             ComputeInheritedLanguage().LowerASCII());
  String inner_text = GetLocale().QueryString(message_id);
  CHECK(message_id);
  permission_text_span()->setInnerText(inner_text);

  UpdateIcon(mojom::blink::PermissionName::WEB_APP_INSTALLATION);
}

void HTMLInstallElement::UpdateIcon(
    mojom::blink::PermissionName permission_name) {
  permission_internal_icon()->SetIcon(show_as_launch_
                                          ? PermissionIconType::kLaunch
                                          : PermissionIconType::kInstall);
}

bool HTMLInstallElement::IsURLAttribute(const Attribute& attr) const {
  return attr.GetName() == html_names::kManifestidAttr ||
         attr.GetName() == html_names::kInstallurlAttr ||
         HTMLElement::IsURLAttribute(attr);
}

void HTMLInstallElement::DefaultEventHandler(Event& event) {
  // We'll handle activation here, and punt everything else through
  // `HTMLCapabilityElementBase`.
  if (event.type() == event_type_names::kDOMActivate) {
    HandleActivation(event, blink::BindOnce(&HTMLInstallElement::OnActivated,
                                            WrapWeakPersistent(this)));
    return;
  }
  HTMLCapabilityElementBase::DefaultEventHandler(event);
}

HeapMojoRemote<mojom::blink::WebInstallService>&
HTMLInstallElement::WebInstallService() {
  // Can be nullptr. e.g. in unit tests, or after document Shutdown().
  auto* context = GetDocument().GetExecutionContext();
  if (!context) {
    return service_;
  }

  if (!service_.is_bound()) {
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
  if (!WebInstallService().is_bound()) {
    // Do nothing if the document's execution context is gone.
    return;
  }

  // If no attributes provided, install current document.
  if (InstallUrl().empty() && ManifestId().empty()) {
    WebInstallService()->InstallFromElement(
        /*options=*/nullptr, BindOnce(&HTMLInstallElement::OnInstallResult,
                                      WrapWeakPersistent(this)));
    return;
  }

  mojom::blink::InstallOptionsPtr options = GetCheckedInstallOptions();
  if (!options) {
    // TODO(crbug.com/481519343): Add long-term solution for error handling (a
    // separate error attribute linked to the install result, etc.).
    // Disable the element to prevent future activations and inform the
    // developer.
    HandleInstallDataError();
    DispatchEvent(
        *Event::CreateCancelableBubble(event_type_names::kPromptdismiss));
    return;
  }

  WebInstallService()->InstallFromElement(
      std::move(options),
      BindOnce(&HTMLInstallElement::OnInstallResult, WrapWeakPersistent(this)));
}

mojom::blink::InstallOptionsPtr HTMLInstallElement::GetCheckedInstallOptions() {
  mojom::blink::InstallOptionsPtr options;

  KURL install_url = KURL(InstallUrl());
  if (!install_url.IsValid()) {
    return nullptr;
  }
  options = mojom::blink::InstallOptions::New();
  options->install_url = install_url;
  // TODO(crbug.com469801429): Evaluate how to handle manifestid validation
  // and resolution.
  // TODO(crbug.com/469940918): Evaluate whether to accept manifestid alone.
  // manifestid is only used if installurl was also provided, as it's used
  // for data validation on the installurl. manifestid match check is handled
  // by WebInstallUrlCommand.
  KURL manifest_id_url = KURL(ManifestId());
  if (manifest_id_url.IsValid()) {
    options->manifest_id = manifest_id_url;
  }
  return options;
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
      // TODO(crbug.com/481519343): Revisit how to best surface this for
      // <install> as a long-term solution (a separate error attribute linked to
      // the install result, etc.).
      // Disable the element to prevent future activations and inform the
      // developer.
      HandleInstallDataError();
      DispatchEvent(
          *Event::CreateCancelableBubble(event_type_names::kPromptdismiss));
      break;
    case mojom::blink::WebInstallServiceResult::kSuccess:
      DispatchEvent(
          *Event::CreateCancelableBubble(event_type_names::kPromptaction));
      break;
  }
}

}  // namespace blink
