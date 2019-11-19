/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Waldo Bastian (bastian@kde.org)
    Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
    Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.

    This class provides all functionality needed for loading images, style
    sheets and html pages from the web. It has a memory cache for these objects.
*/

#include "third_party/blink/renderer/core/loader/resource/script_resource.h"

#include <utility>

#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/loader/request_context_frame_type.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/loader/subresource_integrity_helper.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/web_memory_allocator_dump.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/web_process_memory_dump.h"
#include "third_party/blink/renderer/platform/loader/fetch/cached_metadata.h"
#include "third_party/blink/renderer/platform/loader/fetch/data_pipe_bytes_consumer.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/integrity_metadata.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_client_walker.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader.h"
#include "third_party/blink/renderer/platform/loader/fetch/response_body_loader.h"
#include "third_party/blink/renderer/platform/loader/fetch/script_cached_metadata_handler.h"
#include "third_party/blink/renderer/platform/loader/fetch/text_resource_decoder_options.h"
#include "third_party/blink/renderer/platform/loader/subresource_integrity.h"
#include "third_party/blink/renderer/platform/network/mime/mime_type_registry.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"

namespace blink {

namespace {

// Returns true if the given request context is a script-like destination
// defined in the Fetch spec:
// https://fetch.spec.whatwg.org/#request-destination-script-like
bool IsRequestContextSupported(mojom::RequestContextType request_context) {
  // TODO(nhiroki): Support |kRequestContextSharedWorker| for module loading for
  // shared workers (https://crbug.com/824646).
  // TODO(nhiroki): Support "audioworklet" and "paintworklet" destinations.
  switch (request_context) {
    case mojom::RequestContextType::SCRIPT:
    case mojom::RequestContextType::WORKER:
    case mojom::RequestContextType::SERVICE_WORKER:
      return true;
    default:
      break;
  }
  NOTREACHED() << "Incompatible request context type: " << request_context;
  return false;
}

}  // namespace

ScriptResource* ScriptResource::Fetch(FetchParameters& params,
                                      ResourceFetcher* fetcher,
                                      ResourceClient* client,
                                      StreamingAllowed streaming_allowed) {
  DCHECK(IsRequestContextSupported(
      params.GetResourceRequest().GetRequestContext()));
  ScriptResource* resource = ToScriptResource(
      fetcher->RequestResource(params, ScriptResourceFactory(), client));

  if (streaming_allowed == kAllowStreaming) {
    // Start streaming the script as soon as we get it.
    if (RuntimeEnabledFeatures::ScriptStreamingOnPreloadEnabled()) {
      resource->StartStreaming(fetcher->GetTaskRunner());
    }
  } else {
    // Advance the |streaming_state_| to kStreamingNotAllowed by calling
    // SetClientIsWaitingForFinished unless it is explicitly allowed.'
    //
    // Do this in a task rather than directly to make sure that we don't call
    // the finished callbacks of other clients synchronously.

    // TODO(leszeks): Previous behaviour, without script streaming, was to
    // synchronously notify the given client, with the assumption that other
    // clients were already finished. If this behaviour becomes necessary, we
    // would have to either check that streaming wasn't started (if that would
    // be a logic error), or cancel any existing streaming.
    fetcher->GetTaskRunner()->PostTask(
        FROM_HERE, WTF::Bind(&ScriptResource::SetClientIsWaitingForFinished,
                             WrapWeakPersistent(resource)));
  }

  return resource;
}

ScriptResource* ScriptResource::CreateForTest(
    const KURL& url,
    const WTF::TextEncoding& encoding) {
  ResourceRequest request(url);
  request.SetCredentialsMode(network::mojom::CredentialsMode::kOmit);
  ResourceLoaderOptions options;
  TextResourceDecoderOptions decoder_options(
      TextResourceDecoderOptions::kPlainTextContent, encoding);
  return MakeGarbageCollected<ScriptResource>(request, options,
                                              decoder_options);
}

ScriptResource::ScriptResource(
    const ResourceRequest& resource_request,
    const ResourceLoaderOptions& options,
    const TextResourceDecoderOptions& decoder_options)
    : TextResource(resource_request,
                   ResourceType::kScript,
                   options,
                   decoder_options) {}

ScriptResource::~ScriptResource() = default;

void ScriptResource::Prefinalize() {
  // Reset and cancel the watcher. This has to be called in the prefinalizer,
  // rather than relying on the destructor, as accesses by the watcher of the
  // script resource between prefinalization and destruction are invalid. See
  // https://crbug.com/905975#c34 for more details.
  watcher_.reset();
}

void ScriptResource::Trace(blink::Visitor* visitor) {
  visitor->Trace(streamer_);
  visitor->Trace(response_body_loader_client_);
  TextResource::Trace(visitor);
}

void ScriptResource::OnMemoryDump(WebMemoryDumpLevelOfDetail level_of_detail,
                                  WebProcessMemoryDump* memory_dump) const {
  Resource::OnMemoryDump(level_of_detail, memory_dump);
  const String name = GetMemoryDumpName() + "/decoded_script";
  source_text_.OnMemoryDump(memory_dump, name);
}

const ParkableString& ScriptResource::SourceText() {
  CHECK(IsFinishedInternal());

  if (source_text_.IsNull() && Data()) {
    String source_text = DecodedText();
    ClearData();
    SetDecodedSize(source_text.CharactersSizeInBytes());
    source_text_ = ParkableString(source_text.ReleaseImpl());
  }

  return source_text_;
}

String ScriptResource::TextForInspector() const {
  // If the resource buffer exists, we can safely return the decoded text.
  if (ResourceBuffer())
    return DecodedText();

  // If there is no resource buffer, then we have three cases.
  // TODO(crbug.com/865098): Simplify the below code and remove the CHECKs once
  // the assumptions are confirmed.

  if (IsLoaded()) {
    if (!source_text_.IsNull()) {
      // 1. We have finished loading, and have already decoded the buffer into
      //    the source text and cleared the resource buffer to save space.
      return source_text_.ToString();
    }

    // 2. We have finished loading with no data received, so no streaming ever
    //    happened or streaming was suppressed. Note that the finished
    //    notification may not have come through yet because of task posting, so
    //    NotifyFinished may not have been called yet. Regardless, there was no
    //    data, so the text should be empty.
    //
    // TODO(crbug/909858) Currently this CHECK can occasionally fail, but this
    // doesn't seem to cause real issues immediately. For now, we suppress the
    // crashes on release builds by making this a DCHECK and continue baking the
    // script streamer control (crbug/865098) on beta, while investigating the
    // failure reason on canary.
    DCHECK(!IsFinishedInternal() || !streamer_ ||
           streamer_->StreamingSuppressedReason() ==
               ScriptStreamer::kScriptTooSmall);
    return "";
  }

  // 3. We haven't started loading, and actually haven't received any data yet
  //    at all to initialise the resource buffer, so the resource is empty.
  return "";
}

SingleCachedMetadataHandler* ScriptResource::CacheHandler() {
  return static_cast<SingleCachedMetadataHandler*>(Resource::CacheHandler());
}

CachedMetadataHandler* ScriptResource::CreateCachedMetadataHandler(
    std::unique_ptr<CachedMetadataSender> send_callback) {
  return MakeGarbageCollected<ScriptCachedMetadataHandler>(
      Encoding(), std::move(send_callback));
}

void ScriptResource::SetSerializedCachedMetadata(mojo_base::BigBuffer data) {
  // Resource ignores the cached metadata.
  Resource::SetSerializedCachedMetadata(mojo_base::BigBuffer());
  ScriptCachedMetadataHandler* cache_handler =
      static_cast<ScriptCachedMetadataHandler*>(Resource::CacheHandler());
  if (cache_handler) {
    cache_handler->SetSerializedCachedMetadata(std::move(data));
  }
}

void ScriptResource::DestroyDecodedDataForFailedRevalidation() {
  source_text_ = ParkableString();
  // Make sure there's no streaming.
  DCHECK(!streamer_);
  DCHECK_EQ(streaming_state_, StreamingState::kStreamingNotAllowed);
  SetDecodedSize(0);
}

void ScriptResource::SetRevalidatingRequest(const ResourceRequest& request) {
  CHECK_EQ(streaming_state_, StreamingState::kFinishedNotificationSent);
  if (streamer_) {
    CHECK(streamer_->IsStreamingFinished());
    streamer_ = nullptr;
  }
  // Revalidation requests don't actually load the current Resource, so disable
  // streaming.
  not_streaming_reason_ = ScriptStreamer::kRevalidate;
  streaming_state_ = StreamingState::kStreamingNotAllowed;
  CheckStreamingState();

  TextResource::SetRevalidatingRequest(request);
}

bool ScriptResource::CanUseCacheValidator() const {
  // Do not revalidate until ClassicPendingScript is removed, i.e. the script
  // content is retrieved in ScriptLoader::ExecuteScriptBlock().
  // crbug.com/692856
  if (HasClientsOrObservers())
    return false;

  // Do not revalidate until streaming is complete.
  if (!IsFinishedInternal())
    return false;

  return Resource::CanUseCacheValidator();
}

void ScriptResource::ResponseBodyReceived(
    ResponseBodyLoaderDrainableInterface& body_loader,
    scoped_refptr<base::SingleThreadTaskRunner> loader_task_runner) {
  ResponseBodyLoaderClient* response_body_loader_client;
  CHECK(!data_pipe_);
  data_pipe_ = body_loader.DrainAsDataPipe(&response_body_loader_client);
  if (!data_pipe_)
    return;

  response_body_loader_client_ = response_body_loader_client;
  watcher_ = std::make_unique<mojo::SimpleWatcher>(
      FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL, loader_task_runner);

  watcher_->Watch(data_pipe_.get(), MOJO_HANDLE_SIGNAL_READABLE,
                  MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
                  WTF::BindRepeating(&ScriptResource::OnDataPipeReadable,
                                     WrapWeakPersistent(this)));
  CHECK(data_pipe_);

  MojoResult ready_result;
  mojo::HandleSignalsState ready_state;
  MojoResult rv = watcher_->Arm(&ready_result, &ready_state);
  if (rv == MOJO_RESULT_OK)
    return;

  DCHECK_EQ(MOJO_RESULT_FAILED_PRECONDITION, rv);
  OnDataPipeReadable(ready_result, ready_state);
}

void ScriptResource::OnDataPipeReadable(MojoResult result,
                                        const mojo::HandleSignalsState& state) {
  switch (result) {
    case MOJO_RESULT_OK:
      // All good, so read the data that we were notified that we received.
      break;

    case MOJO_RESULT_CANCELLED:
      // The consumer handle got closed, which means this script resource is
      // done loading, and did so without streaming (otherwise the watcher
      // wouldn't have been armed, and the handle ownership would have passed to
      // the streamer).
      CHECK(streaming_state_ == StreamingState::kFinishedNotificationSent ||
            streaming_state_ == StreamingState::kStreamingNotAllowed);
      return;

    case MOJO_RESULT_FAILED_PRECONDITION:
      // This means the producer finished and streamed to completion.
      watcher_.reset();
      response_body_loader_client_->DidFinishLoadingBody();
      response_body_loader_client_ = nullptr;
      return;

    case MOJO_RESULT_SHOULD_WAIT:
      NOTREACHED();
      return;

    default:
      // Some other error occurred.
      watcher_.reset();
      response_body_loader_client_->DidFailLoadingBody();
      response_body_loader_client_ = nullptr;
      return;
  }
  CHECK(state.readable());
  CHECK(data_pipe_);

  const void* data;
  uint32_t data_size;
  MojoReadDataFlags flags_to_pass = MOJO_READ_DATA_FLAG_NONE;
  MojoResult begin_read_result =
      data_pipe_->BeginReadData(&data, &data_size, flags_to_pass);
  // There should be data, so this read should succeed.
  CHECK_EQ(begin_read_result, MOJO_RESULT_OK);

  response_body_loader_client_->DidReceiveData(
      base::make_span(reinterpret_cast<const char*>(data), data_size));

  MojoResult end_read_result = data_pipe_->EndReadData(data_size);

  CHECK_EQ(end_read_result, MOJO_RESULT_OK);

  CheckStreamingState();
  if (streamer_) {
    DCHECK_EQ(streaming_state_, StreamingState::kStreaming);
    if (streamer_->TryStartStreaming(&data_pipe_,
                                     response_body_loader_client_.Get())) {
      CHECK(!data_pipe_);
      // This reset will also cancel the watcher.
      watcher_.reset();
      return;
    }
  }

  // TODO(leszeks): Depending on how small the chunks are, we may want to
  // loop until a certain number of bytes are synchronously read rather than
  // going back to the scheduler.
  watcher_->ArmOrNotify();
}

void ScriptResource::NotifyFinished() {
  DCHECK(IsLoaded());
  switch (streaming_state_) {
    case StreamingState::kCanStartStreaming:
      // Do nothing, expect either a StartStreaming() call to transition us to
      // kStreaming, or an SetClientIsWaitingForFinished() call to transition us
      // into kStreamingNotAllowed. These will then transition again since
      // IsLoaded will be true.
      break;
    case StreamingState::kStreaming:
      AdvanceStreamingState(StreamingState::kWaitingForStreamingToEnd);
      DCHECK(streamer_);
      streamer_->NotifyFinished();
      // Don't call the base NotifyFinished until streaming finishes too (which
      // might happen immediately in the above ScriptStreamer::NotifyFinished
      // call)
      break;
    case StreamingState::kStreamingNotAllowed:
      watcher_.reset();
      data_pipe_.reset();
      response_body_loader_client_ = nullptr;
      AdvanceStreamingState(StreamingState::kFinishedNotificationSent);
      TextResource::NotifyFinished();
      break;
    case StreamingState::kWaitingForStreamingToEnd:
    case StreamingState::kFinishedNotificationSent:
      // Not possible.
      CHECK(false);
      break;
  }
}

bool ScriptResource::IsFinishedInternal() const {
  CheckStreamingState();
  return streaming_state_ == StreamingState::kFinishedNotificationSent;
}

void ScriptResource::StreamingFinished() {
  CHECK(streamer_);
  CHECK_EQ(streaming_state_, StreamingState::kWaitingForStreamingToEnd);
  CHECK(!data_pipe_ || streamer_->StreamingSuppressed());
  // We may still have a watcher if a) streaming never started (e.g. script too
  // small) and b) an external error triggered the finished notification.
  watcher_.reset();
  data_pipe_.reset();
  response_body_loader_client_ = nullptr;
  AdvanceStreamingState(StreamingState::kFinishedNotificationSent);
  TextResource::NotifyFinished();
}

void ScriptResource::StartStreaming(
    scoped_refptr<base::SingleThreadTaskRunner> loading_task_runner) {
  CheckStreamingState();

  if (streamer_) {
    return;
  }

  if (streaming_state_ != StreamingState::kCanStartStreaming) {
    return;
  }

  // Don't bother streaming if there was an error, it won't work anyway.
  if (ErrorOccurred()) {
    return;
  }

  static const bool script_streaming_enabled =
      base::FeatureList::IsEnabled(features::kScriptStreaming);
  if (!script_streaming_enabled) {
    return;
  }

  CHECK(!IsCacheValidator());

  streamer_ =
      ScriptStreamer::Create(this, loading_task_runner, &not_streaming_reason_);
  if (streamer_) {
    AdvanceStreamingState(StreamingState::kStreaming);

    // If there is any data already, send it to the streamer.
    if (Data()) {
      // Note that we don't need to iterate through the segments of the data, as
      // the streamer will do that itself.
      CHECK_GT(Data()->size(), 0u);
      if (data_pipe_) {
        if (streamer_->TryStartStreaming(&data_pipe_,
                                         response_body_loader_client_.Get())) {
          CHECK(!data_pipe_);
          // This reset will also cancel the watcher.
          watcher_.reset();
        } else {
          CHECK(data_pipe_);
        }
      }
    }
    // If the we're is already loaded, notify the streamer about that too.
    if (IsLoaded()) {
      AdvanceStreamingState(StreamingState::kWaitingForStreamingToEnd);

      // Do this in a task rather than directly to make sure that we don't call
      // the finished callback in the same stack as starting streaming -- this
      // can cause issues with the client expecting to be not finished when
      // starting streaming (e.g. ClassicPendingScript::IsReady == false), but
      // ending up finished by the end of this method.
      loading_task_runner->PostTask(FROM_HERE,
                                    WTF::Bind(&ScriptStreamer::NotifyFinished,
                                              WrapPersistent(streamer_.Get())));
    }
  }

  CheckStreamingState();
  return;
}

void ScriptResource::SetClientIsWaitingForFinished() {
  // No-op if streaming already started or finished.
  CheckStreamingState();
  if (streaming_state_ != StreamingState::kCanStartStreaming)
    return;

  AdvanceStreamingState(StreamingState::kStreamingNotAllowed);
  not_streaming_reason_ = ScriptStreamer::kStreamingDisabled;
  // Trigger the finished notification if needed.
  if (IsLoaded()) {
    watcher_.reset();
    data_pipe_.reset();
    response_body_loader_client_ = nullptr;
    AdvanceStreamingState(StreamingState::kFinishedNotificationSent);
    TextResource::NotifyFinished();
  }
}

ScriptStreamer* ScriptResource::TakeStreamer() {
  CHECK(IsFinishedInternal());
  if (!streamer_)
    return nullptr;

  ScriptStreamer* streamer = streamer_;
  streamer_ = nullptr;
  not_streaming_reason_ = ScriptStreamer::kSecondScriptResourceUse;
  return streamer;
}

void ScriptResource::AdvanceStreamingState(StreamingState new_state) {
  switch (streaming_state_) {
    case StreamingState::kCanStartStreaming:
      CHECK(new_state == StreamingState::kStreaming ||
            new_state == StreamingState::kStreamingNotAllowed);
      break;
    case StreamingState::kStreaming:
      CHECK(streamer_);
      CHECK_EQ(new_state, StreamingState::kWaitingForStreamingToEnd);
      break;
    case StreamingState::kWaitingForStreamingToEnd:
      CHECK(streamer_);
      CHECK_EQ(new_state, StreamingState::kFinishedNotificationSent);
      break;
    case StreamingState::kStreamingNotAllowed:
      CHECK_EQ(new_state, StreamingState::kFinishedNotificationSent);
      break;
    case StreamingState::kFinishedNotificationSent:
      CHECK(false);
      break;
  }

  streaming_state_ = new_state;
  CheckStreamingState();
}

void ScriptResource::CheckStreamingState() const {
  // TODO(leszeks): Eventually convert these CHECKs into DCHECKs once the logic
  // is a bit more baked in.
  switch (streaming_state_) {
    case StreamingState::kCanStartStreaming:
      CHECK(!streamer_);
      break;
    case StreamingState::kStreaming:
      CHECK(streamer_);
      CHECK(!streamer_->IsFinished());
      // kStreaming can be entered both when loading (if streaming is started
      // before load completes) or when loaded (if streaming is started after
      // load completes). In the latter case, the state will almost immediately
      // advance to kWaitingForStreamingToEnd.
      CHECK(IsLoaded() || IsLoading());
      break;
    case StreamingState::kWaitingForStreamingToEnd:
      CHECK(streamer_);
      CHECK(!streamer_->IsFinished());
      CHECK(IsLoaded());
      break;
    case StreamingState::kStreamingNotAllowed:
      CHECK(!streamer_);
      // TODO(leszeks): We could CHECK(!IsLoaded()) if not for the immediate
      // kCanStartStreaming -> kStreamingNotAllowed -> kFinishedNotificationSent
      // transition in SetClientIsWaitingForFinished when IsLoaded.
      break;
    case StreamingState::kFinishedNotificationSent:
      CHECK(!streamer_ || streamer_->IsFinished());
      CHECK(!watcher_ || !watcher_->IsWatching());
      CHECK(!data_pipe_);
      CHECK(!response_body_loader_client_);
      CHECK(IsLoaded());
      break;
  }
}

}  // namespace blink
