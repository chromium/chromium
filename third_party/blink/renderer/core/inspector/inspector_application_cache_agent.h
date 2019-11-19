/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_APPLICATION_CACHE_AGENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_APPLICATION_CACHE_AGENT_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/inspector/inspector_base_agent.h"
#include "third_party/blink/renderer/core/inspector/protocol/ApplicationCache.h"
#include "third_party/blink/renderer/core/loader/appcache/application_cache_host_for_frame.h"

namespace blink {

class LocalFrame;
class InspectedFrames;

class CORE_EXPORT InspectorApplicationCacheAgent final
    : public InspectorBaseAgent<protocol::ApplicationCache::Metainfo> {
 public:
  static InspectorApplicationCacheAgent* Create(
      InspectedFrames* inspected_frames) {
    return MakeGarbageCollected<InspectorApplicationCacheAgent>(
        inspected_frames);
  }

  explicit InspectorApplicationCacheAgent(InspectedFrames*);
  ~InspectorApplicationCacheAgent() override = default;
  void Trace(blink::Visitor*) override;

  // InspectorBaseAgent
  void Restore() override;
  protocol::Response disable() override;

  // InspectorInstrumentation API
  void UpdateApplicationCacheStatus(LocalFrame*);
  void NetworkStateChanged(LocalFrame*, bool online);

  // ApplicationCache API for frontend
  protocol::Response getFramesWithManifests(
      std::unique_ptr<
          protocol::Array<protocol::ApplicationCache::FrameWithManifest>>*
          frame_ids) override;
  protocol::Response enable() override;
  protocol::Response getManifestForFrame(const String& frame_id,
                                         String* manifest_url) override;
  protocol::Response getApplicationCacheForFrame(
      const String& frame_id,
      std::unique_ptr<protocol::ApplicationCache::ApplicationCache>*) override;

 private:
  // Unconditionally enables the agent, even if |enabled_.Get()==true|.
  // For idempotence, call enable().
  void InnerEnable();

  std::unique_ptr<protocol::ApplicationCache::ApplicationCache>
  BuildObjectForApplicationCache(
      const Vector<mojom::blink::AppCacheResourceInfo>&,
      const ApplicationCacheHost::CacheInfo&);
  std::unique_ptr<
      protocol::Array<protocol::ApplicationCache::ApplicationCacheResource>>
  BuildArrayForApplicationCacheResources(
      const Vector<mojom::blink::AppCacheResourceInfo>&);
  std::unique_ptr<protocol::ApplicationCache::ApplicationCacheResource>
  BuildObjectForApplicationCacheResource(
      const mojom::blink::AppCacheResourceInfo&);

  protocol::Response AssertFrameWithDocumentLoader(String frame_id,
                                                   DocumentLoader*&);

  Member<InspectedFrames> inspected_frames_;
  InspectorAgentState::Boolean enabled_;
  DISALLOW_COPY_AND_ASSIGN(InspectorApplicationCacheAgent);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_INSPECTOR_APPLICATION_CACHE_AGENT_H_
