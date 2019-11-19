// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/cache_storage/cache_utils.h"

#include "third_party/blink/renderer/core/fetch/fetch_response_data.h"
#include "third_party/blink/renderer/core/fetch/response.h"
#include "third_party/blink/renderer/modules/cache_storage/cache_storage_blob_client_list.h"
#include "third_party/blink/renderer/platform/loader/fetch/data_pipe_bytes_consumer.h"

namespace blink {

Response* CreateEagerResponse(ScriptState* script_state,
                              mojom::blink::EagerResponsePtr eager_response,
                              CacheStorageBlobClientList* client_list) {
  auto& response = eager_response->response;
  DCHECK(!response->blob);

  ExecutionContext* context = ExecutionContext::From(script_state);

  FetchResponseData* fetch_data =
      Response::CreateUnfilteredFetchResponseDataWithoutBody(script_state,
                                                             *response);

  DataPipeBytesConsumer::CompletionNotifier* completion_notifier = nullptr;
  fetch_data->ReplaceBodyStreamBuffer(MakeGarbageCollected<BodyStreamBuffer>(
      script_state,
      MakeGarbageCollected<DataPipeBytesConsumer>(
          context->GetTaskRunner(TaskType::kNetworking),
          std::move(eager_response->pipe), &completion_notifier),
      nullptr /* AbortSignal */));

  // Create a BlobReaderClient in the provided list.  This will track the
  // completion of the eagerly read blob and propagate it to the given
  // DataPipeBytesConsumer::CompletionNotifier.  The list will also hold
  // the client alive.
  client_list->AddClient(std::move(eager_response->client_receiver),
                         std::move(completion_notifier));

  fetch_data = Response::FilterResponseData(
      response->response_type, fetch_data, response->cors_exposed_header_names);

  return Response::Create(context, fetch_data);
}

}  // namespace blink
