/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "third_party/blink/renderer/core/inspector/inspector_application_cache_agent.h"

#include "third_party/blink/public/mojom/appcache/appcache_info.mojom-blink.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/inspector/inspected_frames.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/platform/network/network_state_notifier.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {
using protocol::Response;

InspectorApplicationCacheAgent::InspectorApplicationCacheAgent(
    InspectedFrames* inspected_frames)
    : inspected_frames_(inspected_frames),
      enabled_(&agent_state_, /*default_value=*/false) {}

void InspectorApplicationCacheAgent::InnerEnable() {
  enabled_.Set(true);
  instrumenting_agents_->AddInspectorApplicationCacheAgent(this);
  GetFrontend()->networkStateUpdated(GetNetworkStateNotifier().OnLine());
}

void InspectorApplicationCacheAgent::Restore() {
  if (enabled_.Get())
    InnerEnable();
}

Response InspectorApplicationCacheAgent::enable() {
  if (!enabled_.Get())
    InnerEnable();
  return Response::OK();
}

Response InspectorApplicationCacheAgent::disable() {
  enabled_.Clear();
  instrumenting_agents_->RemoveInspectorApplicationCacheAgent(this);
  return Response::OK();
}

void InspectorApplicationCacheAgent::UpdateApplicationCacheStatus(
    LocalFrame* frame) {
  DocumentLoader* document_loader = frame->Loader().GetDocumentLoader();
  if (!document_loader)
    return;

  ApplicationCacheHostForFrame* host =
      document_loader->GetApplicationCacheHost();
  mojom::AppCacheStatus status = host->GetStatus();
  ApplicationCacheHost::CacheInfo info = host->ApplicationCacheInfo();

  String manifest_url = info.manifest_.GetString();
  String frame_id = IdentifiersFactory::FrameId(frame);
  GetFrontend()->applicationCacheStatusUpdated(frame_id, manifest_url,
                                               static_cast<int>(status));
}

void InspectorApplicationCacheAgent::NetworkStateChanged(LocalFrame* frame,
                                                         bool online) {
  if (frame == inspected_frames_->Root())
    GetFrontend()->networkStateUpdated(online);
}

Response InspectorApplicationCacheAgent::getFramesWithManifests(
    std::unique_ptr<
        protocol::Array<protocol::ApplicationCache::FrameWithManifest>>*
        result) {
  *result = std::make_unique<
      protocol::Array<protocol::ApplicationCache::FrameWithManifest>>();

  for (LocalFrame* frame : *inspected_frames_) {
    DocumentLoader* document_loader = frame->Loader().GetDocumentLoader();
    if (!document_loader)
      continue;

    ApplicationCacheHostForFrame* host =
        document_loader->GetApplicationCacheHost();
    ApplicationCacheHost::CacheInfo info = host->ApplicationCacheInfo();
    String manifest_url = info.manifest_.GetString();
    if (!manifest_url.IsEmpty()) {
      std::unique_ptr<protocol::ApplicationCache::FrameWithManifest> value =
          protocol::ApplicationCache::FrameWithManifest::create()
              .setFrameId(IdentifiersFactory::FrameId(frame))
              .setManifestURL(manifest_url)
              .setStatus(static_cast<int>(host->GetStatus()))
              .build();
      (*result)->emplace_back(std::move(value));
    }
  }
  return Response::OK();
}

Response InspectorApplicationCacheAgent::AssertFrameWithDocumentLoader(
    String frame_id,
    DocumentLoader*& result) {
  LocalFrame* frame =
      IdentifiersFactory::FrameById(inspected_frames_, frame_id);
  if (!frame)
    return Response::Error("No frame for given id found");

  result = frame->Loader().GetDocumentLoader();
  if (!result)
    return Response::Error("No documentLoader for given frame found");
  return Response::OK();
}

Response InspectorApplicationCacheAgent::getManifestForFrame(
    const String& frame_id,
    String* manifest_url) {
  DocumentLoader* document_loader = nullptr;
  Response response = AssertFrameWithDocumentLoader(frame_id, document_loader);
  if (!response.isSuccess())
    return response;

  ApplicationCacheHost::CacheInfo info =
      document_loader->GetApplicationCacheHost()->ApplicationCacheInfo();
  *manifest_url = info.manifest_.GetString();
  return Response::OK();
}

Response InspectorApplicationCacheAgent::getApplicationCacheForFrame(
    const String& frame_id,
    std::unique_ptr<protocol::ApplicationCache::ApplicationCache>*
        application_cache) {
  DocumentLoader* document_loader = nullptr;
  Response response = AssertFrameWithDocumentLoader(frame_id, document_loader);
  if (!response.isSuccess())
    return response;

  ApplicationCacheHostForFrame* host =
      document_loader->GetApplicationCacheHost();
  ApplicationCacheHost::CacheInfo info = host->ApplicationCacheInfo();

  Vector<mojom::blink::AppCacheResourceInfo> resources;
  host->FillResourceList(&resources);

  *application_cache = BuildObjectForApplicationCache(resources, info);
  return Response::OK();
}

std::unique_ptr<protocol::ApplicationCache::ApplicationCache>
InspectorApplicationCacheAgent::BuildObjectForApplicationCache(
    const Vector<mojom::blink::AppCacheResourceInfo>&
        application_cache_resources,
    const ApplicationCacheHost::CacheInfo& application_cache_info) {
  return protocol::ApplicationCache::ApplicationCache::create()
      .setManifestURL(application_cache_info.manifest_.GetString())
      .setSize(application_cache_info.response_sizes_)
      .setCreationTime(application_cache_info.creation_time_)
      .setUpdateTime(application_cache_info.update_time_)
      .setResources(
          BuildArrayForApplicationCacheResources(application_cache_resources))
      .build();
}

std::unique_ptr<
    protocol::Array<protocol::ApplicationCache::ApplicationCacheResource>>
InspectorApplicationCacheAgent::BuildArrayForApplicationCacheResources(
    const Vector<mojom::blink::AppCacheResourceInfo>&
        application_cache_resources) {
  auto resources = std::make_unique<
      protocol::Array<protocol::ApplicationCache::ApplicationCacheResource>>();

  for (const auto& resource : application_cache_resources)
    resources->emplace_back(BuildObjectForApplicationCacheResource(resource));

  return resources;
}

std::unique_ptr<protocol::ApplicationCache::ApplicationCacheResource>
InspectorApplicationCacheAgent::BuildObjectForApplicationCacheResource(
    const mojom::blink::AppCacheResourceInfo& resource_info) {
  StringBuilder builder;
  if (resource_info.is_master)
    builder.Append("Master ");

  if (resource_info.is_manifest)
    builder.Append("Manifest ");

  if (resource_info.is_fallback)
    builder.Append("Fallback ");

  if (resource_info.is_foreign)
    builder.Append("Foreign ");

  if (resource_info.is_explicit)
    builder.Append("Explicit ");

  std::unique_ptr<protocol::ApplicationCache::ApplicationCacheResource> value =
      protocol::ApplicationCache::ApplicationCacheResource::create()
          .setUrl(resource_info.url.GetString())
          .setSize(static_cast<int>(resource_info.response_size))
          .setType(builder.ToString())
          .build();
  return value;
}

void InspectorApplicationCacheAgent::Trace(blink::Visitor* visitor) {
  visitor->Trace(inspected_frames_);
  InspectorBaseAgent::Trace(visitor);
}

}  // namespace blink
