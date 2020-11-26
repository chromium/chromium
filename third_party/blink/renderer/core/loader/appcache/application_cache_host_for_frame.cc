// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/appcache/application_cache_host_for_frame.h"

#include "third_party/blink/public/common/loader/url_loader_factory_bundle.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/events/application_cache_error_event.h"
#include "third_party/blink/renderer/core/events/progress_event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/loader/appcache/application_cache.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/web_test_support.h"

namespace blink {

namespace {

const char kHttpGETMethod[] = "GET";

KURL ClearUrlRef(const KURL& input_url) {
  KURL url(input_url);
  if (!url.HasFragmentIdentifier())
    return url;
  url.RemoveFragmentIdentifier();
  return url;
}

void RestartNavigation(LocalFrame* frame) {
  LocalDOMWindow* window = frame->DomWindow();
  FrameLoadRequest request(window, ResourceRequest(window->Url()));
  request.SetClientRedirectReason(ClientNavigationReason::kReload);
  frame->Navigate(request, WebFrameLoadType::kReplaceCurrentItem);
}

}  // namespace

ApplicationCacheHostForFrame::ApplicationCacheHostForFrame(
    DocumentLoader* document_loader,
    const BrowserInterfaceBrokerProxy& interface_broker_proxy,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const base::UnguessableToken& appcache_host_id)
    : ApplicationCacheHost(interface_broker_proxy, std::move(task_runner)),
      local_frame_(document_loader->GetFrame()),
      document_loader_(document_loader) {
  // PlzNavigate: The browser passes the ID to be used.
  if (!appcache_host_id.is_empty())
    SetHostID(appcache_host_id);
}

void ApplicationCacheHostForFrame::Detach() {
  ApplicationCacheHost::Detach();
  document_loader_ = nullptr;
  SetApplicationCache(nullptr);
}

bool ApplicationCacheHostForFrame::Update() {
  if (!backend_host_.is_bound())
    return false;

  bool result = false;
  backend_host_->StartUpdate(&result);
  if (!result)
    return false;
  if (status_ == mojom::blink::AppCacheStatus::APPCACHE_STATUS_IDLE ||
      status_ == mojom::blink::AppCacheStatus::APPCACHE_STATUS_UPDATE_READY) {
    status_ = mojom::blink::AppCacheStatus::APPCACHE_STATUS_CHECKING;
  } else {
    status_ = mojom::blink::AppCacheStatus::APPCACHE_STATUS_UNCACHED;
    backend_host_->GetStatus(&status_);
  }
  return true;
}

bool ApplicationCacheHostForFrame::SwapCache() {
  if (!backend_host_.is_bound())
    return false;

  bool success = false;
  backend_host_->SwapCache(&success);
  if (!success)
    return false;
  backend_host_->GetStatus(&status_);
  probe::UpdateApplicationCacheStatus(document_loader_->GetFrame());
  return true;
}

void ApplicationCacheHostForFrame::SetApplicationCache(
    ApplicationCache* dom_application_cache) {
  DCHECK(!dom_application_cache_ || !dom_application_cache);
  dom_application_cache_ = dom_application_cache;
}

void ApplicationCacheHostForFrame::StopDeferringEvents() {
  for (unsigned i = 0; i < deferred_events_.size(); ++i) {
    const DeferredEvent& deferred = deferred_events_[i];
    DispatchDOMEvent(deferred.event_id, deferred.progress_total,
                     deferred.progress_done, deferred.error_reason,
                     deferred.error_url, deferred.error_status,
                     deferred.error_message);
  }
  deferred_events_.clear();
  defers_events_ = false;
}

void ApplicationCacheHostForFrame::LogMessage(
    mojom::blink::ConsoleMessageLevel log_level,
    const String& message) {
  if (WebTestSupport::IsRunningWebTest())
    return;

  if (!local_frame_ || !local_frame_->IsMainFrame())
    return;

  Frame* main_frame = local_frame_->GetPage()->MainFrame();
  if (!main_frame->IsLocalFrame())
    return;
  // TODO(michaeln): Make app cache host per-frame and correctly report to the
  // involved frame.
  auto* local_frame = DynamicTo<LocalFrame>(main_frame);
  local_frame->GetDocument()->AddConsoleMessage(
      MakeGarbageCollected<ConsoleMessage>(mojom::ConsoleMessageSource::kOther,
                                           log_level, message));
}

void ApplicationCacheHostForFrame::SetSubresourceFactory(
    mojo::PendingRemote<network::mojom::blink::URLLoaderFactory>
        url_loader_factory) {
  auto pending_factories = std::make_unique<PendingURLLoaderFactoryBundle>();
  pending_factories->pending_appcache_factory() =
      ToCrossVariantMojoType(std::move(url_loader_factory));
  local_frame_->Client()->UpdateSubresourceFactory(
      std::move(pending_factories));
}

void ApplicationCacheHostForFrame::WillStartLoadingMainResource(
    DocumentLoader* loader,
    const KURL& url,
    const String& method) {
  if (!IsApplicationCacheEnabled())
    return;

  if (GetHostID().is_empty()) {
    // If we did not get host id from the browser, we postpone creation of a new
    // one until this point, where we actually load non-empty document.
    SetHostID(base::UnguessableToken::Create());
  }

  // We defer binding to backend to avoid unnecessary binding around creating
  // empty documents. At this point, we're initiating a main resource load for
  // the document, so its for real.
  if (!BindBackend())
    return;

  original_main_resource_url_ = ClearUrlRef(url);
  is_get_method_ = (method == kHttpGETMethod);
  DCHECK(method == method.UpperASCII());

  const ApplicationCacheHost* spawning_host = nullptr;

  DCHECK(loader->GetFrame());
  LocalFrame* frame = loader->GetFrame();
  Frame* spawning_frame = frame->Tree().Parent();
  if (!spawning_frame || !IsA<LocalFrame>(spawning_frame))
    spawning_frame = frame->Loader().Opener();
  if (!spawning_frame || !IsA<LocalFrame>(spawning_frame))
    spawning_frame = frame;
  if (DocumentLoader* spawning_doc_loader =
          To<LocalFrame>(spawning_frame)->Loader().GetDocumentLoader()) {
    spawning_host = spawning_doc_loader->GetApplicationCacheHost();
  }

  if (spawning_host && (spawning_host != this) &&
      (spawning_host->GetStatus() !=
       mojom::blink::AppCacheStatus::APPCACHE_STATUS_UNCACHED)) {
    backend_host_->SetSpawningHostId(spawning_host->GetHostID());
  }
}

void ApplicationCacheHostForFrame::SelectCacheWithoutManifest() {
  if (!backend_host_.is_bound())
    return;

  if (was_select_cache_called_)
    return;
  was_select_cache_called_ = true;

  status_ =
      (document_response_.AppCacheID() == mojom::blink::kAppCacheNoCacheId)
          ? mojom::blink::AppCacheStatus::APPCACHE_STATUS_UNCACHED
          : mojom::blink::AppCacheStatus::APPCACHE_STATUS_CHECKING;
  is_new_master_entry_ = OLD_ENTRY;
  backend_host_->SelectCache(document_url_, document_response_.AppCacheID(),
                             KURL());
}

void ApplicationCacheHostForFrame::SelectCacheWithManifest(
    const KURL& manifest_url) {
  LocalFrame* frame = document_loader_->GetFrame();
  LocalDOMWindow* window = frame->DomWindow();
  if (window->IsSandboxed(network::mojom::blink::WebSandboxFlags::kOrigin)) {
    // Prevent sandboxes from establishing application caches.
    SelectCacheWithoutManifest();
    return;
  }
  CHECK(window->IsSecureContext());
  Deprecation::CountDeprecation(
      window, WebFeature::kApplicationCacheManifestSelectSecureOrigin);

  if (!backend_host_.is_bound())
    return;

  if (was_select_cache_called_)
    return;
  was_select_cache_called_ = true;

  KURL manifest_kurl(ClearUrlRef(manifest_url));

  // 6.9.6 The application cache selection algorithm
  // Check for new 'master' entries.
  if (document_response_.AppCacheID() == mojom::blink::kAppCacheNoCacheId) {
    if (is_scheme_supported_ && is_get_method_ &&
        SecurityOrigin::AreSameOrigin(manifest_kurl, document_url_)) {
      status_ = mojom::blink::AppCacheStatus::APPCACHE_STATUS_CHECKING;
      is_new_master_entry_ = NEW_ENTRY;
    } else {
      status_ = mojom::blink::AppCacheStatus::APPCACHE_STATUS_UNCACHED;
      is_new_master_entry_ = OLD_ENTRY;
      manifest_kurl = KURL();
    }
    backend_host_->SelectCache(document_url_, mojom::blink::kAppCacheNoCacheId,
                               manifest_kurl);
    return;
  }

  DCHECK_EQ(OLD_ENTRY, is_new_master_entry_);

  // 6.9.6 The application cache selection algorithm
  // Check for 'foreign' entries.
  KURL document_manifest_kurl(document_response_.AppCacheManifestURL());
  if (document_manifest_kurl != manifest_kurl) {
    backend_host_->MarkAsForeignEntry(document_url_,
                                      document_response_.AppCacheID());
    status_ = mojom::blink::AppCacheStatus::APPCACHE_STATUS_UNCACHED;
    // It's a foreign entry, restart the current navigation from the top of the
    // navigation algorithm. The navigation will not result in the same resource
    // being loaded, because "foreign" entries are never picked during
    // navigation. see ApplicationCacheGroup::selectCache()
    RestartNavigation(local_frame_);  // the navigation will be restarted
    return;
  }

  status_ = mojom::blink::AppCacheStatus::APPCACHE_STATUS_CHECKING;

  // It's a 'master' entry that's already in the cache.
  backend_host_->SelectCache(document_url_, document_response_.AppCacheID(),
                             manifest_kurl);
}

void ApplicationCacheHostForFrame::DidReceiveResponseForMainResource(
    const ResourceResponse& response) {
  if (!backend_host_.is_bound())
    return;

  document_response_ = response;
  document_url_ = ClearUrlRef(document_response_.CurrentRequestUrl());
  if (document_url_ != original_main_resource_url_)
    is_get_method_ = true;  // A redirect was involved.
  original_main_resource_url_ = KURL();

  is_scheme_supported_ =
      Platform::Current()->IsURLSupportedForAppCache(document_url_);
  if ((document_response_.AppCacheID() != mojom::blink::kAppCacheNoCacheId) ||
      !is_scheme_supported_ || !is_get_method_)
    is_new_master_entry_ = OLD_ENTRY;
}

void ApplicationCacheHostForFrame::Trace(Visitor* visitor) const {
  visitor->Trace(dom_application_cache_);
  visitor->Trace(local_frame_);
  visitor->Trace(document_loader_);
  ApplicationCacheHost::Trace(visitor);
}

void ApplicationCacheHostForFrame::NotifyApplicationCache(
    mojom::AppCacheEventID id,
    int progress_total,
    int progress_done,
    mojom::AppCacheErrorReason error_reason,
    const String& error_url,
    int error_status,
    const String& error_message) {
  if (id != mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT) {
    probe::UpdateApplicationCacheStatus(document_loader_->GetFrame());
  }

  if (defers_events_) {
    // Event dispatching is deferred until document.onload has fired.
    deferred_events_.push_back(DeferredEvent(id, progress_total, progress_done,
                                             error_reason, error_url,
                                             error_status, error_message));
    return;
  }
  DispatchDOMEvent(id, progress_total, progress_done, error_reason, error_url,
                   error_status, error_message);
}

void ApplicationCacheHostForFrame::DispatchDOMEvent(
    mojom::AppCacheEventID id,
    int progress_total,
    int progress_done,
    mojom::AppCacheErrorReason error_reason,
    const String& error_url,
    int error_status,
    const String& error_message) {
  // Don't dispatch an event if the window is detached.
  if (!dom_application_cache_ || !dom_application_cache_->DomWindow())
    return;

  const AtomicString& event_type = ApplicationCache::ToEventType(id);
  if (event_type.IsEmpty())
    return;
  Event* event = nullptr;
  if (id == mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT) {
    event =
        ProgressEvent::Create(event_type, true, progress_done, progress_total);
  } else if (id == mojom::AppCacheEventID::APPCACHE_ERROR_EVENT) {
    event = MakeGarbageCollected<ApplicationCacheErrorEvent>(
        error_reason, error_url, error_status, error_message);
  } else {
    event = Event::Create(event_type);
  }
  dom_application_cache_->DispatchEvent(*event);
}

bool ApplicationCacheHostForFrame::IsApplicationCacheEnabled() {
  DCHECK(document_loader_->GetFrame());
  return document_loader_->GetFrame()->GetSettings() &&
         document_loader_->GetFrame()
             ->GetSettings()
             ->GetOfflineWebApplicationCacheEnabled();
}

}  // namespace blink
