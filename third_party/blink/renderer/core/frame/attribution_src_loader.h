// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_ATTRIBUTION_SRC_LOADER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_ATTRIBUTION_SRC_LOADER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/inspector/inspector_audits_issue.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/forward.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class HTMLElement;
class HTMLImageElement;
class KURL;
class LocalFrame;

class CORE_EXPORT AttributionSrcLoader
    : public GarbageCollected<AttributionSrcLoader> {
 public:
  enum class RegisterResult {
    kSuccess,
    kInvalidProtocol,
    kNotAllowed,
    kInsecureContext,
    kUntrustworthyOrigin,
  };

  explicit AttributionSrcLoader(LocalFrame* frame);
  AttributionSrcLoader(const AttributionSrcLoader&) = delete;
  AttributionSrcLoader& operator=(const AttributionSrcLoader&) = delete;
  AttributionSrcLoader(AttributionSrcLoader&& other) = delete;
  AttributionSrcLoader& operator=(AttributionSrcLoader&& other) = delete;
  ~AttributionSrcLoader();

  // Registers an attribution_src. This method handles fetching the attribution
  // src and notifying the browser process to begin tracking it. It is a no-op
  // if the frame is not attached.
  RegisterResult Register(const KURL& attribution_src,
                          HTMLImageElement* element);

  void Trace(Visitor* visitor) const;

 private:
  class ResourceClient;

  void DoRegistration(const KURL& src_url);

  void LogAuditIssue(AttributionReportingIssueType issue_type,
                     const String& string,
                     HTMLElement* element = nullptr);

  const Member<LocalFrame> local_frame_;
  HeapHashSet<Member<ResourceClient>> resource_clients_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_ATTRIBUTION_SRC_LOADER_H_
