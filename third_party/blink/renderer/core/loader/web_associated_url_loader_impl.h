// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_WEB_ASSOCIATED_URL_LOADER_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_WEB_ASSOCIATED_URL_LOADER_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/single_thread_task_runner.h"
#include "third_party/blink/public/web/web_associated_url_loader.h"
#include "third_party/blink/public/web/web_associated_url_loader_options.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

class ThreadableLoader;
class WebAssociatedURLLoaderClient;
class Document;

// This class is used to implement WebFrame::createAssociatedURLLoader.
class CORE_EXPORT WebAssociatedURLLoaderImpl final
    : public WebAssociatedURLLoader {
  USING_FAST_MALLOC(WebAssociatedURLLoaderImpl);

 public:
  WebAssociatedURLLoaderImpl(Document*, const WebAssociatedURLLoaderOptions&);
  ~WebAssociatedURLLoaderImpl() override;

  void LoadAsynchronously(const WebURLRequest&,
                          WebAssociatedURLLoaderClient*) override;
  void Cancel() override;
  void SetDefersLoading(bool) override;
  void SetLoadingTaskRunner(base::SingleThreadTaskRunner*) override;

  // Called by |observer_| to handle destruction of the Document associated
  // with the frame given to the constructor.
  void DocumentDestroyed();

  // Called by ClientAdapter to handle completion of loading.
  void ClientAdapterDone();

 private:
  class ClientAdapter;
  class Observer;

  void CancelLoader();
  void DisposeObserver();

  WebAssociatedURLLoaderClient* ReleaseClient() {
    WebAssociatedURLLoaderClient* client = client_;
    client_ = nullptr;
    return client;
  }

  WebAssociatedURLLoaderClient* client_;
  WebAssociatedURLLoaderOptions options_;

  // Converts ThreadableLoaderClient method calls into WebURLLoaderClient method
  // calls.
  Persistent<ClientAdapter> client_adapter_;
  Persistent<ThreadableLoader> loader_;

  // A ContextLifecycleObserver for cancelling |loader_| when the Document is
  // detached.
  Persistent<Observer> observer_;

  DISALLOW_COPY_AND_ASSIGN(WebAssociatedURLLoaderImpl);
};

}  // namespace blink

#endif
