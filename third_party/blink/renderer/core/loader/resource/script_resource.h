/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller <mueller@kde.org>
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_RESOURCE_SCRIPT_RESOURCE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_RESOURCE_SCRIPT_RESOURCE_H_

#include <memory>

#include "third_party/blink/renderer/bindings/core/v8/script_streamer.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/loader/resource/text_resource.h"
#include "third_party/blink/renderer/platform/bindings/parkable_string.h"
#include "third_party/blink/renderer/platform/loader/fetch/integrity_metadata.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_client.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/fetch/text_resource_decoder_options.h"

namespace mojo {
class SimpleWatcher;
}

namespace blink {

class FetchParameters;
class KURL;
class ResourceFetcher;
class ResponseBodyLoaderClient;
class SingleCachedMetadataHandler;

// ScriptResource is a resource representing a JavaScript script. It is only
// used for "classic" scripts, i.e. not modules.
//
// In addition to loading the script, a ScriptResource can optionally stream the
// script to the JavaScript parser/compiler, using a ScriptStreamer. In this
// case, clients of the ScriptResource will not receive the finished
// notification until the streaming completes.
//
// See also:
// https://docs.google.com/document/d/143GOPl_XVgLPFfO-31b_MdBcnjklLEX2OIg_6eN6fQ4
class CORE_EXPORT ScriptResource final : public TextResource {
  USING_PRE_FINALIZER(ScriptResource, Prefinalize);

 public:
  // For scripts fetched with kAllowStreaming, the ScriptResource expects users
  // to call StartStreaming to start streaming the loaded data, and
  // SetClientIsWaitingForFinished when they actually want the data to be
  // available for execute. Note that StartStreaming can fail, so the client of
  // an unfinished resource has to call SetClientIsWaitingForFinished to
  // guarantee that it receives a finished callback.
  //
  // Scripts fetched with kNoStreaming will (asynchronously) call
  // SetClientIsWaitingForFinished on the resource, so the user does not have to
  // call it again. This is effectively the "legacy" behaviour.
  enum StreamingAllowed { kNoStreaming, kAllowStreaming };

  static ScriptResource* Fetch(FetchParameters&,
                               ResourceFetcher*,
                               ResourceClient*,
                               StreamingAllowed);

  // Public for testing
  static ScriptResource* CreateForTest(const KURL& url,
                                       const WTF::TextEncoding& encoding);

  ScriptResource(const ResourceRequest&,
                 const ResourceLoaderOptions&,
                 const TextResourceDecoderOptions&);
  ~ScriptResource() override;

  void ResponseBodyReceived(
      ResponseBodyLoaderDrainableInterface& body_loader,
      scoped_refptr<base::SingleThreadTaskRunner> loader_task_runner) override;

  void Trace(blink::Visitor*) override;

  void OnMemoryDump(WebMemoryDumpLevelOfDetail,
                    WebProcessMemoryDump*) const override;

  void SetSerializedCachedMetadata(mojo_base::BigBuffer data) override;

  void StartStreaming(
      scoped_refptr<base::SingleThreadTaskRunner> loading_task_runner);

  // State that a client of the script resource will no longer try to start
  // streaming, and is now waiting for the resource to call the client's finish
  // callback (regardless of whether the resource is finished loading or
  // finished streaming). Specifically, it causes the kCanStartStreaming to
  // kStreamingNotAllowed transition. Streaming cannot be started after this is
  // called.
  //
  // If the resource is already streaming, this will be a no-op, and the client
  // will still only get the finished notification when the streaming completes.
  //
  // This function should never be called synchronously (except by
  // NotifyFinished) as it can trigger all clients' finished callbacks, which in
  // turn can invoke JavaScript execution.
  //
  // TODO(leszeks): Eventually Fetch (with streaming allowed) will be the only
  // way of starting streaming, and SetClientIsWaitingForFinished will not be
  // part of the public interface.
  void SetClientIsWaitingForFinished();

  // Called (only) by ScriptStreamer when streaming completes.
  //
  // This function should never be called synchronously as it can trigger all
  // clients' finished callbacks, which in turn can invoke JavaScript execution.
  void StreamingFinished();

  const ParkableString& SourceText();

  // Get the resource's current text. This can return partial data, so should
  // not be used outside of the inspector.
  String TextForInspector() const;

  SingleCachedMetadataHandler* CacheHandler();

  // Gets the script streamer from the ScriptResource, clearing the resource's
  // streamer so that it cannot be used twice.
  ScriptStreamer* TakeStreamer();

  ScriptStreamer::NotStreamingReason NoStreamerReason() const {
    return not_streaming_reason_;
  }

  // Used in DCHECKs
  bool HasStreamer() { return !!streamer_; }
  bool HasRunningStreamer() { return streamer_ && !streamer_->IsFinished(); }
  bool HasFinishedStreamer() { return streamer_ && streamer_->IsFinished(); }

  // Visible for tests.
  void SetRevalidatingRequest(const ResourceRequest&) override;

 protected:
  CachedMetadataHandler* CreateCachedMetadataHandler(
      std::unique_ptr<CachedMetadataSender> send_callback) override;

  void DestroyDecodedDataForFailedRevalidation() override;

  // ScriptResources are considered finished when either:
  //   1. Loading + streaming completes, or
  //   2. Loading completes + streaming was never started + someone called
  //      "SetClientIsWaitingForFinished" to block streaming from ever starting.
  void NotifyFinished() override;
  bool IsFinishedInternal() const override;

 private:
  // Valid state transitions:
  //
  // kCanStartStreaming -> kStreaming -> kWaitingForStreamingToEnd
  //                                                -> kFinishedNotificationSent
  // kCanStartStreaming -> kStreamingNotAllowed -> kFinishedNotificationSent
  enum class StreamingState {
    kCanStartStreaming,         // Streaming can be started.
    kStreamingNotAllowed,       // Streaming can no longer be started.
    kStreaming,                 // Both loading the resource and streaming.
    kWaitingForStreamingToEnd,  // Resource loaded but streaming not complete.
    kFinishedNotificationSent   // Everything complete and finish sent.
  };

  class ScriptResourceFactory : public ResourceFactory {
   public:
    ScriptResourceFactory()
        : ResourceFactory(ResourceType::kScript,
                          TextResourceDecoderOptions::kPlainTextContent) {}

    Resource* Create(
        const ResourceRequest& request,
        const ResourceLoaderOptions& options,
        const TextResourceDecoderOptions& decoder_options) const override {
      return MakeGarbageCollected<ScriptResource>(request, options,
                                                  decoder_options);
    }
  };

  void Prefinalize();

  bool CanUseCacheValidator() const override;

  void AdvanceStreamingState(StreamingState new_state);

  // Check that invariants for the state hold.
  void CheckStreamingState() const;

  void OnDataPipeReadable(MojoResult result,
                          const mojo::HandleSignalsState& state);

  ParkableString source_text_;

  mojo::ScopedDataPipeConsumerHandle data_pipe_;
  std::unique_ptr<mojo::SimpleWatcher> watcher_;
  Member<ResponseBodyLoaderClient> response_body_loader_client_;

  Member<ScriptStreamer> streamer_;
  ScriptStreamer::NotStreamingReason not_streaming_reason_ =
      ScriptStreamer::kDidntTryToStartStreaming;
  StreamingState streaming_state_ = StreamingState::kCanStartStreaming;
};

DEFINE_RESOURCE_TYPE_CASTS(Script);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_RESOURCE_SCRIPT_RESOURCE_H_
