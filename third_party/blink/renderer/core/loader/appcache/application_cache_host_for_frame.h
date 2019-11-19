// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_APPCACHE_APPLICATION_CACHE_HOST_FOR_FRAME_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_APPCACHE_APPLICATION_CACHE_HOST_FOR_FRAME_H_

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/loader/appcache/application_cache_host.h"

namespace blink {

class ApplicationCache;
class DocumentLoader;
class LocalFrame;

class CORE_EXPORT ApplicationCacheHostForFrame : public ApplicationCacheHost {
 public:
  ApplicationCacheHostForFrame(
      DocumentLoader* document_loader,
      const BrowserInterfaceBrokerProxy& interface_broker_proxy,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      const base::UnguessableToken& appcache_host_id);

  // ApplicationCacheHost:
  void Detach() override;

  bool Update();
  bool SwapCache();
  void SetApplicationCache(ApplicationCache*);
  void StopDeferringEvents();

  // blink::mojom::AppCacheFrontend:
  void LogMessage(mojom::blink::ConsoleMessageLevel log_level,
                  const String& message) override;
  void SetSubresourceFactory(
      mojo::PendingRemote<network::mojom::blink::URLLoaderFactory>
          url_loader_factory) override;

  void WillStartLoadingMainResource(DocumentLoader* loader,
                                    const KURL& url,
                                    const String& method);
  virtual void SelectCacheWithoutManifest();
  void SelectCacheWithManifest(const KURL& manifest_url);
  void DidReceiveResponseForMainResource(const ResourceResponse&);

  void Trace(blink::Visitor*) override;

 private:
  enum IsNewMasterEntry { MAYBE_NEW_ENTRY, NEW_ENTRY, OLD_ENTRY };

  struct DeferredEvent {
    mojom::AppCacheEventID event_id;
    int progress_total;
    int progress_done;
    mojom::AppCacheErrorReason error_reason;
    String error_url;
    int error_status;
    String error_message;
    DeferredEvent(mojom::AppCacheEventID id,
                  int progress_total,
                  int progress_done,
                  mojom::AppCacheErrorReason error_reason,
                  const String& error_url,
                  int error_status,
                  const String& error_message)
        : event_id(id),
          progress_total(progress_total),
          progress_done(progress_done),
          error_reason(error_reason),
          error_url(error_url),
          error_status(error_status),
          error_message(error_message) {}
  };

  void NotifyApplicationCache(mojom::AppCacheEventID,
                              int progress_total,
                              int progress_done,
                              mojom::AppCacheErrorReason,
                              const String& error_url,
                              int error_status,
                              const String& error_message) override;

  void DispatchDOMEvent(mojom::AppCacheEventID,
                        int progress_total,
                        int progress_done,
                        mojom::AppCacheErrorReason,
                        const String& error_url,
                        int error_status,
                        const String& error_message);

  bool IsApplicationCacheEnabled();

  WeakMember<ApplicationCache> dom_application_cache_ = nullptr;

  Member<LocalFrame> local_frame_;
  Member<DocumentLoader> document_loader_;

  bool is_get_method_ = false;
  bool was_select_cache_called_ = false;
  IsNewMasterEntry is_new_master_entry_ = MAYBE_NEW_ENTRY;
  bool is_scheme_supported_ = false;
  ResourceResponse document_response_;
  KURL document_url_;
  KURL original_main_resource_url_;  // Used to detect redirection.

  // Events are deferred until after document onload.
  bool defers_events_ = true;
  Vector<DeferredEvent> deferred_events_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_APPCACHE_APPLICATION_CACHE_HOST_FOR_FRAME_H_
