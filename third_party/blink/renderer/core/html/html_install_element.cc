// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_install_element.h"

#include "base/notreached.h"
#include "third_party/blink/public/mojom/web_install/web_install.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/strings/grit/permission_element_strings.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_install_result.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_install_result_event_init.h"
#include "third_party/blink/renderer/core/dom/attribute.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/html_capability_element_base.h"
#include "third_party/blink/renderer/core/html/html_permission_icon_element.h"
#include "third_party/blink/renderer/core/html/install_result_event.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mojo/mojo_binding_context.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

// TODO(crbug.com/534847491): Narrow kAbortError to user cancellation; the set
// of result cases handled here may change.
HTMLInstallElement::InstallResult ToInstallResult(
    mojom::blink::WebInstallServiceResult result) {
  switch (result) {
    case mojom::blink::WebInstallServiceResult::kAbortError:
      return HTMLInstallElement::InstallResult::kAbortError;
    case mojom::blink::WebInstallServiceResult::kDataError:
      return HTMLInstallElement::InstallResult::kDataError;
    case mojom::blink::WebInstallServiceResult::kSuccess:
      return HTMLInstallElement::InstallResult::kSuccess;
  }
  NOTREACHED();
}

V8InstallResult::Enum ToV8InstallResult(
    HTMLInstallElement::InstallResult result) {
  switch (result) {
    case HTMLInstallElement::InstallResult::kAbortError:
      return V8InstallResult::Enum::kAborted;
    case HTMLInstallElement::InstallResult::kDataError:
      return V8InstallResult::Enum::kInvalidData;
    case HTMLInstallElement::InstallResult::kSuccess:
      return V8InstallResult::Enum::kSuccess;
  }
  NOTREACHED();
}

}  // namespace

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

void HTMLInstallElement::Trace(Visitor* visitor) const {
  visitor->Trace(service_);
  HTMLCapabilityElementBase::Trace(visitor);
}

void HTMLInstallElement::UpdateAppearance() {
  if (IsInCanvasSubtree()) {
    OnIsInstalledResult(false);
    return;
  }

  // If no attributes were provided, check if the current document is already
  // installed.
  if (!FastHasAttribute(html_names::kManifestidAttr) &&
      !FastHasAttribute(html_names::kManifestAttr)) {
    // TODO(crbug.com/485281836): For now, always return false while we discuss
    // the appropriate long-term mitigation for width-based side channel
    // attacks. ("Launch" is slightly wider than "Install").
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
  show_as_launch_ = is_installed && !IsInCanvasSubtree();

  // This is posted as a task, as similar code in
  // `HTMLGeolocationElement::UpdateAppearance` would crash due to DCHECKs being
  // hit for calling setInnerText during layout.
  // TODO(crbug.com/477974745): If possible, bind the mojo pipe to a task runner
  // that cannot be called during layout, to avoid this and simplify
  // <geolocation> too.
  GetDocument()
      .GetTaskRunner(TaskType::kInternalDefault)
      ->PostTask(FROM_HERE,
                 BindOnce(&HTMLInstallElement::UpdateAppearanceTask,
                          WrapWeakPersistent(this), show_as_launch_));
}

void HTMLInstallElement::UpdateAppearanceTask(bool is_installed) {
  is_installed = is_installed && !IsInCanvasSubtree();
  show_as_launch_ = is_installed;

  // TODO(crbug.com/467103133): Render site-specific information.
  uint16_t message_id =
      GetTranslatedMessageID(is_installed ? IDS_PERMISSION_REQUEST_LAUNCH
                                          : IDS_PERMISSION_REQUEST_INSTALL,
                             ComputeInheritedLanguage().ToAsciiLower());
  String inner_text = GetLocale().QueryString(message_id);
  CHECK(message_id);
  permission_text_span()->setInnerText(inner_text);
  UpdateIcon(mojom::blink::PermissionName::WEB_APP_INSTALLATION);
}

void HTMLInstallElement::UpdateIcon(mojom::blink::PermissionName permission) {
  permission_internal_icon()->SetIcon(show_as_launch_
                                          ? PermissionIconType::kLaunch
                                          : PermissionIconType::kInstall);
}

bool HTMLInstallElement::IsURLAttribute(const Attribute& attr) const {
  return attr.GetName() == html_names::kManifestidAttr ||
         attr.GetName() == html_names::kManifestAttr ||
         HTMLElement::IsURLAttribute(attr);
}

void HTMLInstallElement::DidChangeIsInCanvasSubtree() {
  HTMLCapabilityElementBase::DidChangeIsInCanvasSubtree();
  UpdateAppearance();
}

void HTMLInstallElement::RunActivationBehavior(
    Event& event,
    EventDispatchHandlingState* handling_state) {
  if (!RuntimeEnabledFeatures::CleanUpActivationBehaviorEnabled()) {
    HTMLCapabilityElementBase::RunActivationBehavior(event, handling_state);
    return;
  }
  if (event.defaultPrevented() || event.DefaultHandled()) {
    return;
  }
  HandleActivation(event, blink::BindOnce(&HTMLInstallElement::OnActivated,
                                          WrapWeakPersistent(this)));
}

void HTMLInstallElement::DefaultEventHandler(Event& event) {
  if (!RuntimeEnabledFeatures::CleanUpActivationBehaviorEnabled()) {
    // We'll handle activation here, and punt everything else through
    // `HTMLCapabilityElementBase`.
    if (event.type() == event_type_names::kDOMActivate) {
      HandleActivation(event, blink::BindOnce(&HTMLInstallElement::OnActivated,
                                              WrapWeakPersistent(this)));
      return;
    }
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

  if (FastHasAttribute(html_names::kInstallurlAttr)) {
    DispatchInstallResultEvent(InstallResult::kDataError);
    return;
  }

  // Only absent target attributes select the current-document flow. A present
  // but empty manifest should instead reach attribute validation and fail.
  if (!FastHasAttribute(html_names::kManifestidAttr) &&
      !FastHasAttribute(html_names::kManifestAttr)) {
    WebInstallService()->ElementInstallFromManifest(
        /*options=*/nullptr,
        BindOnce(&HTMLInstallElement::OnManifestInstallResult,
                 WrapWeakPersistent(this)));
    return;
  }

  // A manifest attribute was set (but may still be invalid). Initiate the
  // browser's manifest install flow, which directly fetches the manifest file.
  if (FastHasAttribute(html_names::kManifestAttr)) {
    mojom::blink::ManifestInstallOptionsPtr options =
        GetCheckedManifestInstallOptions();
    if (!options) {
      DispatchInstallResultEvent(InstallResult::kDataError);
      return;
    }

    WebInstallService()->ElementInstallFromManifest(
        std::move(options),
        BindOnce(&HTMLInstallElement::OnManifestInstallResult,
                 WrapWeakPersistent(this)));
    return;
  }

  // If we get here, only the manifest ID was set, which is considered an error
  // case.
  DispatchInstallResultEvent(InstallResult::kDataError);
}

mojom::blink::ManifestInstallOptionsPtr
HTMLInstallElement::GetCheckedManifestInstallOptions() {
  // Read the raw attribute, strip surrounding whitespace, and resolve the URL
  // if possible.
  KURL manifest_url = GetNonEmptyURLAttribute(html_names::kManifestAttr);
  if (!manifest_url.IsValid()) {
    return nullptr;
  }

  auto options = mojom::blink::ManifestInstallOptions::New();
  options->manifest_url = manifest_url;

  // manifest ID is optional, but must be non-empty and valid when present.
  if (FastHasAttribute(html_names::kManifestidAttr)) {
    StringView manifest_id = StripLeadingAndTrailingHtmlSpaces(
        FastGetAttribute(html_names::kManifestidAttr));
    if (manifest_id.empty()) {
      return nullptr;
    }
    KURL manifest_id_url = KURL(GetDocument().BaseURL(), manifest_id);
    if (!manifest_id_url.IsValid()) {
      return nullptr;
    }
    options->manifest_id = manifest_id_url;
  }

  return options;
}

void HTMLInstallElement::DispatchInstallResultEvent(
    const InstallResult result) {
  auto* event_init = InstallResultEventInit::Create();
  event_init->setResult(V8InstallResult(ToV8InstallResult(result)));
  event_init->setBubbles(true);
  EnqueueEvent(
      *InstallResultEvent::Create(event_type_names::kInstallresult, event_init),
      TaskType::kUserInteraction);
}

void HTMLInstallElement::OnManifestInstallResult(
    mojom::blink::WebInstallServiceResult result) {
  DispatchInstallResultEvent(ToInstallResult(result));
}

}  // namespace blink
