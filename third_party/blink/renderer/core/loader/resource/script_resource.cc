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

#include "services/network/public/mojom/request_context_frame_type.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/loader/subresource_integrity_helper.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/web_memory_allocator_dump.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/web_process_memory_dump.h"
#include "third_party/blink/renderer/platform/loader/fetch/cached_metadata.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/integrity_metadata.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_client_walker.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/script_cached_metadata_handler.h"
#include "third_party/blink/renderer/platform/loader/fetch/text_resource_decoder_options.h"
#include "third_party/blink/renderer/platform/loader/subresource_integrity.h"
#include "third_party/blink/renderer/platform/network/mime/mime_type_registry.h"
#include "third_party/blink/renderer/platform/shared_buffer.h"

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
                                      ResourceClient* client) {
  DCHECK_EQ(params.GetResourceRequest().GetFrameType(),
            network::mojom::RequestContextFrameType::kNone);
  DCHECK(IsRequestContextSupported(
      params.GetResourceRequest().GetRequestContext()));
  return ToScriptResource(
      fetcher->RequestResource(params, ScriptResourceFactory(), client));
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

void ScriptResource::OnMemoryDump(WebMemoryDumpLevelOfDetail level_of_detail,
                                  WebProcessMemoryDump* memory_dump) const {
  Resource::OnMemoryDump(level_of_detail, memory_dump);
  const String name = GetMemoryDumpName() + "/decoded_script";
  auto* dump = memory_dump->CreateMemoryAllocatorDump(name);
  dump->AddScalar("size", "bytes", source_text_.CharactersSizeInBytes());
  memory_dump->AddSuballocation(
      dump->Guid(), String(WTF::Partitions::kAllocatedObjectPoolName));
}

const ParkableString& ScriptResource::SourceText() {
  DCHECK(IsLoaded());

  if (source_text_.IsNull() && Data()) {
    String source_text = DecodedText();
    ClearData();
    SetDecodedSize(source_text.CharactersSizeInBytes());
    source_text_ = ParkableString(source_text.ReleaseImpl());
  }

  return source_text_;
}

SingleCachedMetadataHandler* ScriptResource::CacheHandler() {
  return static_cast<SingleCachedMetadataHandler*>(Resource::CacheHandler());
}

CachedMetadataHandler* ScriptResource::CreateCachedMetadataHandler(
    std::unique_ptr<CachedMetadataSender> send_callback) {
  return new ScriptCachedMetadataHandler(Encoding(), std::move(send_callback));
}

void ScriptResource::SetSerializedCachedMetadata(const char* data,
                                                 size_t size) {
  Resource::SetSerializedCachedMetadata(data, size);
  ScriptCachedMetadataHandler* cache_handler =
      static_cast<ScriptCachedMetadataHandler*>(Resource::CacheHandler());
  if (cache_handler) {
    cache_handler->SetSerializedCachedMetadata(data, size);
  }
}

void ScriptResource::DestroyDecodedDataForFailedRevalidation() {
  source_text_ = ParkableString();
  SetDecodedSize(0);
}

AccessControlStatus ScriptResource::CalculateAccessControlStatus() const {
  DCHECK_NE(GetResponse().GetType(), network::mojom::FetchResponseType::kError);
  return GetResponse().IsCORSSameOrigin() ? kSharableCrossOrigin
                                          : kOpaqueResource;
}

bool ScriptResource::CanUseCacheValidator() const {
  // Do not revalidate until ClassicPendingScript is removed, i.e. the script
  // content is retrieved in ScriptLoader::ExecuteScriptBlock().
  // crbug.com/692856
  if (HasClientsOrObservers())
    return false;

  return Resource::CanUseCacheValidator();
}

}  // namespace blink
