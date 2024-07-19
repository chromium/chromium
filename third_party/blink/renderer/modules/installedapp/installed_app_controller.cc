// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/installedapp/installed_app_controller.h"

#include <utility>

#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/mojom/installedapp/related_application.mojom-blink.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/manifest/manifest_manager.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

InstalledAppController::~InstalledAppController() = default;

void InstalledAppController::GetInstalledRelatedApps(
    std::unique_ptr<AppInstalledCallbacks> callbacks) {
  // When detached, the fetch logic is no longer valid.
  if (!GetSupplementable()->GetFrame()) {
    // TODO(mgiuca): AbortError rather than simply undefined.
    // https://crbug.com/687846
    callbacks->OnError();
    return;
  }

  // Get the list of related applications from the manifest.
  // Upon returning, filter the result list to those apps that are installed.
  ManifestManager::From(*GetSupplementable())
      ->RequestManifest(
          WTF::BindOnce(&InstalledAppController::OnGetManifestForRelatedApps,
                        WrapPersistent(this), std::move(callbacks)));
}

InstalledAppController* InstalledAppController::From(LocalDOMWindow& window) {
  InstalledAppController* controller =
      Supplement<LocalDOMWindow>::From<InstalledAppController>(window);
  if (!controller) {
    controller = MakeGarbageCollected<InstalledAppController>(window);
    Supplement<LocalDOMWindow>::ProvideTo(window, controller);
  }

  return controller;
}

const char InstalledAppController::kSupplementName[] = "InstalledAppController";

InstalledAppController::InstalledAppController(LocalDOMWindow& window)
    : Supplement<LocalDOMWindow>(window),
      provider_(&window) {}

void InstalledAppController::OnGetManifestForRelatedApps(
    std::unique_ptr<AppInstalledCallbacks> callbacks,
    mojom::blink::ManifestRequestResult result,
    const KURL& url,
    mojom::blink::ManifestPtr manifest) {
  if (!GetSupplementable()->GetFrame()) {
    callbacks->OnError();
    return;
  }
  Vector<mojom::blink::RelatedApplicationPtr> mojo_related_apps;
  for (const auto& related_application : manifest->related_applications) {
    auto application = mojom::blink::RelatedApplication::New();
    application->platform = related_application->platform;
    application->id = related_application->id;
    if (related_application->url.has_value())
      application->url = related_application->url->GetString();
    mojo_related_apps.push_back(std::move(application));
  }

  if (!provider_.is_bound()) {
    // See https://bit.ly/2S0zRAS for task types.
    GetSupplementable()->GetBrowserInterfaceBroker().GetInterface(
        provider_.BindNewPipeAndPassReceiver(
            GetSupplementable()->GetTaskRunner(TaskType::kMiscPlatformAPI)));
    // TODO(mgiuca): Set a connection error handler. This requires a refactor to
    // work like NavigatorShare.cpp (retain a persistent list of clients to
    // reject all of their promises).
    DCHECK(provider_.is_bound());
  }

  provider_->FilterInstalledApps(
      std::move(mojo_related_apps), url,
      WTF::BindOnce(&InstalledAppController::OnFilterInstalledApps,
                    WrapPersistent(this), std::move(callbacks)));
}

void InstalledAppController::OnFilterInstalledApps(
    std::unique_ptr<AppInstalledCallbacks> callbacks,
    Vector<mojom::blink::RelatedApplicationPtr> result) {
  HeapVector<Member<RelatedApplication>> applications;
  for (const auto& res : result) {
    auto* app = RelatedApplication::Create();
    app->setPlatform(res->platform);
    if (!res->url.IsNull())
      app->setUrl(res->url);
    if (!res->id.IsNull())
      app->setId(res->id);
    if (!res->version.IsNull())
      app->setVersion(res->version);
    applications.push_back(app);
  }

  LocalDOMWindow* window = GetSupplementable();
  ukm::builders::InstalledRelatedApps(window->UkmSourceID())
      .SetCalled(true)
      .Record(window->UkmRecorder());

  callbacks->OnSuccess(applications);
}

void InstalledAppController::Trace(Visitor* visitor) const {
  visitor->Trace(provider_);
  Supplement<LocalDOMWindow>::Trace(visitor);
}

}  // namespace blink
