// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/installedapp/installed_app_controller.h"

#include <utility>

#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/mojom/installedapp/related_application.mojom-blink.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-blink.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/manifest/manifest_manager.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

InstalledAppController::~InstalledAppController() = default;

void InstalledAppController::GetInstalledRelatedApps(
    std::unique_ptr<AppInstalledCallbacks> callbacks) {
  // When detached, the fetch logic is no longer valid.
  if (!GetExecutionContext()) {
    // TODO(mgiuca): AbortError rather than simply undefined.
    // https://crbug.com/687846
    callbacks->OnError();
    return;
  }

  // Get the list of related applications from the manifest.
  // Upon returning, filter the result list to those apps that are installed.
  ManifestManager::From(*GetSupplementable())
      ->RequestManifest(
          WTF::Bind(&InstalledAppController::OnGetManifestForRelatedApps,
                    WrapPersistent(this), std::move(callbacks)));
}

void InstalledAppController::ProvideTo(LocalFrame& frame) {
  Supplement<LocalFrame>::ProvideTo(
      frame, MakeGarbageCollected<InstalledAppController>(frame));
}

InstalledAppController* InstalledAppController::From(LocalFrame& frame) {
  InstalledAppController* controller =
      Supplement<LocalFrame>::From<InstalledAppController>(frame);
  DCHECK(controller);
  return controller;
}

const char InstalledAppController::kSupplementName[] = "InstalledAppController";

InstalledAppController::InstalledAppController(LocalFrame& frame)
    : Supplement<LocalFrame>(frame),
      ContextLifecycleObserver(frame.GetDocument()) {}

void InstalledAppController::ContextDestroyed(ExecutionContext*) {
  provider_.reset();
}

void InstalledAppController::OnGetManifestForRelatedApps(
    std::unique_ptr<AppInstalledCallbacks> callbacks,
    const KURL& /*url*/,
    mojom::blink::ManifestPtr manifest) {
  Vector<mojom::blink::RelatedApplicationPtr> mojo_related_apps;
  for (const auto& related_application : manifest->related_applications) {
    auto application = mojom::blink::RelatedApplication::New();
    application->platform = related_application->platform;
    application->id = related_application->id;
    if (related_application->url.has_value())
      application->url = related_application->url->GetString();
    mojo_related_apps.push_back(std::move(application));
  }

  if (!provider_) {
    // See https://bit.ly/2S0zRAS for task types.
    GetSupplementable()->GetBrowserInterfaceBroker().GetInterface(
        provider_.BindNewPipeAndPassReceiver(
            GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI)));
    // TODO(mgiuca): Set a connection error handler. This requires a refactor to
    // work like NavigatorShare.cpp (retain a persistent list of clients to
    // reject all of their promises).
    DCHECK(provider_);
  }

  provider_->FilterInstalledApps(
      std::move(mojo_related_apps),
      WTF::Bind(&InstalledAppController::OnFilterInstalledApps,
                WrapPersistent(this), WTF::Passed(std::move(callbacks))));
}

void InstalledAppController::OnFilterInstalledApps(
    std::unique_ptr<AppInstalledCallbacks> callbacks,
    Vector<mojom::blink::RelatedApplicationPtr> result) {
  HeapVector<Member<RelatedApplication>> applications;
  for (const auto& res : result) {
    auto* app = MakeGarbageCollected<RelatedApplication>();
    app->setPlatform(res->platform);
    app->setURL(res->url);
    app->setId(res->id);
    app->setVersion(res->version);
    applications.push_back(app);
  }
  callbacks->OnSuccess(applications);
}

void InstalledAppController::Trace(Visitor* visitor) {
  Supplement<LocalFrame>::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
