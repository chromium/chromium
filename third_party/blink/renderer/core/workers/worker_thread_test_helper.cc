// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/workers/worker_thread_test_helper.h"

#include "base/numerics/safe_conversions.h"
#include "third_party/blink/public/mojom/v8_cache_options.mojom-blink.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/workers/worker_settings.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object_snapshot.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"

namespace blink {

void RunWorkerToplevelScriptForTesting(WorkerThread& worker_thread,
                                       const SecurityOrigin* security_origin,
                                       const String& source,
                                       const KURL& script_url) {
  // We create `outside_settings_object` but this is (mostly) unused because
  // the off-the-main-thread worker script loading uses the
  // `WorkerMainScriptLoadParameters` members below.
  auto& outside_settings_object =
      FetchClientSettingsObjectSnapshot::CreateForTesting(script_url,
                                                          security_origin);

  std::unique_ptr<WorkerMainScriptLoadParameters>
      worker_main_script_load_params =
          std::make_unique<WorkerMainScriptLoadParameters>();

  auto head = network::mojom::URLResponseHead::New();
  head->headers =
      net::HttpResponseHeaders::Builder(net::HttpVersion(1, 1), "200 OK")
          .AddHeader("Content-Type", "text/javascript")
          .Build();
  head->headers->GetMimeType(&head->mime_type);
  worker_main_script_load_params->response_head = std::move(head);

  // `loader` remains pending, as we don't need to handle `URLLLoader`
  // calls.
  mojo::PendingReceiver<network::mojom::URLLoader> loader;
  mojo::Remote<network::mojom::URLLoaderClient> loader_client;
  worker_main_script_load_params->url_loader_client_endpoints =
      network::mojom::URLLoaderClientEndpoints::New(
          loader.InitWithNewPipeAndPassRemote(),
          loader_client.BindNewPipeAndPassReceiver());
  loader_client->OnComplete(network::URLLoaderCompletionStatus(net::OK));

  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  mojo::ScopedDataPipeProducerHandle producer_handle;
  StringUtf8Adaptor utf8_source(source);
  CHECK_EQ(
      mojo::CreateDataPipe(base::checked_cast<uint32_t>(utf8_source.size()),
                           producer_handle, consumer_handle),
      MOJO_RESULT_OK);
  CHECK_EQ(producer_handle->WriteAllData(
               base::as_byte_span(utf8_source.AsStringView())),
           MOJO_RESULT_OK);
  worker_main_script_load_params->response_body = std::move(consumer_handle);

  worker_thread.FetchAndRunClassicScript(
      script_url, std::move(worker_main_script_load_params),
      /*policy_container=*/nullptr, outside_settings_object.CopyData(),
      /*outside_resource_timing_notifier=*/nullptr,
      v8_inspector::V8StackTraceId());
}

}  // namespace blink
