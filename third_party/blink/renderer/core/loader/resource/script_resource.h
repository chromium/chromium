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

#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/mojom/script/script_type.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/script/script_type.mojom-shared.h"
#include "third_party/blink/renderer/bindings/core/v8/script_cache_consumer.h"
#include "third_party/blink/renderer/bindings/core/v8/script_streamer.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/loader/resource/text_resource.h"
#include "third_party/blink/renderer/platform/bindings/parkable_string.h"
#include "third_party/blink/renderer/platform/loader/fetch/integrity_metadata.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_client.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/fetch/text_resource_decoder_options.h"

namespace blink {

class CachedMetadataHandler;
class FetchParameters;
class KURL;
class ResourceFetcher;
class ScriptCachedMetadataHandler;

// ScriptResource is a resource representing a JavaScript, either a classic or
// module script. Based on discussions (crbug.com/1178198) ScriptResources are
// shared between classic and module scripts.
//
// In addition to loading the script, a ScriptResource can optionally stream the
// script to the JavaScript parser/compiler, using a ScriptStreamer. In this
// case, clients of the ScriptResource will not receive the finished
// notification until the streaming completes.
// Note: ScriptStreamer is only used for "classic" scripts, i.e. not modules.
//
// See also:
// https://docs.google.com/document/d/143GOPl_XVgLPFfO-31b_MdBcnjklLEX2OIg_6eN6fQ4
class CORE_EXPORT ScriptResource final : public TextResource {
 public:
  // The script resource will always try to start streaming if kAllowStreaming
  // is passed in.
  enum StreamingAllowed { kNoStreaming, kAllowStreaming };

  static ScriptResource* Fetch(FetchParameters&,
                               ResourceFetcher*,
                               ResourceClient*,
                               StreamingAllowed);

  // Public for testing
  static ScriptResource* CreateForTest(
      const KURL& url,
      const WTF::TextEncoding& encoding,
      mojom::blink::ScriptType = mojom::blink::ScriptType::kClassic);

  ScriptResource(const ResourceRequest&,
                 const ResourceLoaderOptions&,
                 const TextResourceDecoderOptions&,
                 StreamingAllowed,
                 mojom::blink::ScriptType);
  ~ScriptResource() override;

  size_t CodeCacheSize() const override;
  void ResponseReceived(const ResourceResponse&) override;
  void ResponseBodyReceived(
      ResponseBodyLoaderDrainableInterface& body_loader,
      scoped_refptr<base::SingleThreadTaskRunner> loader_task_runner) override;
  void DidReceiveDecodedData(
      const String& data,
      std::unique_ptr<ParkableStringImpl::SecureDigest> digest) override;

  void Trace(Visitor*) const override;

  void OnMemoryDump(WebMemoryDumpLevelOfDetail,
                    WebProcessMemoryDump*) const override;

  void SetSerializedCachedMetadata(mojo_base::BigBuffer data) override;

  bool CodeCacheHashRequired() const override;

  const ParkableString& SourceText();

  // Get the resource's current text. This can return partial data, so should
  // not be used outside of the inspector.
  String TextForInspector() const;

  CachedMetadataHandler* CacheHandler();

  mojom::blink::ScriptType GetInitialRequestScriptType() const {
    return initial_request_script_type_;
  }

  // Gets the script streamer from the ScriptResource, clearing the resource's
  // streamer so that it cannot be used twice.
  ResourceScriptStreamer* TakeStreamer();

  ScriptStreamer::NotStreamingReason NoStreamerReason() const {
    return no_streamer_reason_;
  }

  // Used in DCHECKs
  bool HasStreamer() { return !!streamer_; }
  bool HasRunningStreamer() {
    return streamer_ && streamer_->IsStreamingStarted() &&
           !streamer_->IsFinished();
  }
  bool HasFinishedStreamer() { return streamer_ && streamer_->IsFinished(); }

  // Gets the cache consumer from the ScriptResource, clearing it from the
  // resource so that it cannot be used twice.
  //
  // It's fine to return a non-null ScriptCacheConsumer for one user of
  // ScriptResource while returning null for others, as the ScriptCacheConsumer
  // is associated with individual ScriptResource users and not with the
  // ScriptResource itself.
  ScriptCacheConsumer* TakeCacheConsumer();

  // Visible for tests.
  void SetRevalidatingRequest(const ResourceRequestHead&) override;

 protected:
  void DestroyDecodedDataIfPossible() override;
  void DestroyDecodedDataForFailedRevalidation() override;

  // ScriptResources are considered finished when either:
  //   1. Loading + streaming completes, or
  //   2. Loading completes + streaming is disabled.
  void NotifyFinished() override;

  void SetEncoding(const String& chs) override;

 private:
  // Valid state transitions:
  //
  //            kWaitingForDataPipe          DisableStreaming()
  //                    |---------------------------.
  //                    |                           |
  //                    v                           v
  //               kStreaming -----------------> kStreamingDisabled
  //
  enum class StreamingState {
    // Streaming is allowed on this resource, but we're waiting to receive a
    // data pipe.
    kWaitingForDataPipe,
    // The script streamer is active, and has the data pipe.
    kStreaming,

    // Streaming was disabled, either manually or because we got a body with
    // no data-pipe.
    kStreamingDisabled,
  };

  // Valid state transitions:
  //
  //            kWaitingForCache              DisableOffThreadConsumeCache()
  //                    |---------------------------.
  //                    |                           |
  //                    v                           v
  //            kRunningOffThread ----------> kOffThreadConsumeCacheDisabled
  //
  enum class ConsumeCacheState {
    // No cached data has been received.
    kWaitingForCache,
    // Cache is being consumed off-thread.
    kRunningOffThread,
    // Off-thread consume was disabled, either because it wasn't possible,
    // wasn't allowed, or had completed and the consumer has already been taken.
    kOffThreadConsumeCacheDisabled,
  };

  class ScriptResourceFactory : public ResourceFactory {
   public:
    explicit ScriptResourceFactory(
        StreamingAllowed streaming_allowed,
        mojom::blink::ScriptType initial_request_script_type)
        : ResourceFactory(ResourceType::kScript,
                          TextResourceDecoderOptions::kPlainTextContent),
          streaming_allowed_(streaming_allowed),
          initial_request_script_type_(initial_request_script_type) {}

    Resource* Create(
        const ResourceRequest& request,
        const ResourceLoaderOptions& options,
        const TextResourceDecoderOptions& decoder_options) const override {
      return MakeGarbageCollected<ScriptResource>(
          request, options, decoder_options, streaming_allowed_,
          initial_request_script_type_);
    }

   private:
    StreamingAllowed streaming_allowed_;
    mojom::blink::ScriptType initial_request_script_type_;
  };

  bool CanUseCacheValidator() const override;

  void DisableStreaming(ScriptStreamer::NotStreamingReason no_streamer_reason);

  void AdvanceStreamingState(StreamingState new_state);

  // Check that invariants for the state hold.
  void CheckStreamingState() const;

  void DisableOffThreadConsumeCache();

  void AdvanceConsumeCacheState(ConsumeCacheState new_state);

  // Check that invariants for the state hold.
  void CheckConsumeCacheState() const;

  void OnDataPipeReadable(MojoResult result,
                          const mojo::HandleSignalsState& state);

  ParkableString source_text_;

  Member<ResourceScriptStreamer> streamer_;
  ScriptStreamer::NotStreamingReason no_streamer_reason_ =
      ScriptStreamer::NotStreamingReason::kInvalid;
  StreamingState streaming_state_ = StreamingState::kWaitingForDataPipe;
  Member<ScriptCachedMetadataHandler> cached_metadata_handler_;
  Member<ScriptCacheConsumer> cache_consumer_;
  ConsumeCacheState consume_cache_state_;
  const mojom::blink::ScriptType initial_request_script_type_;
  std::unique_ptr<TextResourceDecoder> stream_text_decoder_;
};

template <>
struct DowncastTraits<ScriptResource> {
  static bool AllowFrom(const Resource& resource) {
    return resource.GetType() == ResourceType::kScript;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_RESOURCE_SCRIPT_RESOURCE_H_
