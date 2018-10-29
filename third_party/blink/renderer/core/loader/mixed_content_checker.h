/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_MIXED_CONTENT_CHECKER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_MIXED_CONTENT_CHECKER_H_

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "third_party/blink/public/platform/web_mixed_content_context_type.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/weborigin/security_violation_reporting_policy.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ConsoleMessage;
class FetchClientSettingsObjectImpl;
class Frame;
class FrameFetchContext;
class LocalFrame;
class KURL;
class ResourceResponse;
class SecurityOrigin;
class SourceLocation;
class WorkerFetchContext;

// Checks resource loads for mixed content. If PlzNavigate is enabled then this
// class only checks for sub-resource loads while frame-level loads are
// delegated to the browser where they are checked by
// MixedContentNavigationThrottle. Changes to this class might need to be
// reflected on its browser counterpart.
//
// Current mixed content W3C draft that drives this implementation:
// https://w3c.github.io/webappsec-mixed-content/
class CORE_EXPORT MixedContentChecker final {
  DISALLOW_NEW();

 public:
  static bool ShouldBlockFetch(LocalFrame*,
                               mojom::RequestContextType,
                               network::mojom::RequestContextFrameType,
                               ResourceRequest::RedirectStatus,
                               const KURL&,
                               SecurityViolationReportingPolicy =
                                   SecurityViolationReportingPolicy::kReport);

  static bool ShouldBlockFetchOnWorker(const WorkerFetchContext&,
                                       mojom::RequestContextType,
                                       ResourceRequest::RedirectStatus,
                                       const KURL&,
                                       SecurityViolationReportingPolicy,
                                       bool is_worklet_global_scope);

  static bool IsWebSocketAllowed(const FrameFetchContext&,
                                 LocalFrame*,
                                 const KURL&);
  static bool IsWebSocketAllowed(const WorkerFetchContext&, const KURL&);

  static bool IsMixedContent(const SecurityOrigin*, const KURL&);
  static bool IsMixedContent(const FetchClientSettingsObjectImpl&, const KURL&);
  static bool IsMixedFormAction(LocalFrame*,
                                const KURL&,
                                SecurityViolationReportingPolicy =
                                    SecurityViolationReportingPolicy::kReport);

  static bool ShouldAutoupgrade(KURL frame_url,
                                WebMixedContentContextType type);

  static void CheckMixedPrivatePublic(LocalFrame*,
                                      const AtomicString& resource_ip_address);

  static WebMixedContentContextType ContextTypeForInspector(
      LocalFrame*,
      const ResourceRequest&);

  // Returns the frame that should be considered the effective frame
  // for a mixed content check for the given frame type.
  static Frame* EffectiveFrameForFrameType(
      LocalFrame*,
      network::mojom::RequestContextFrameType);

  static void HandleCertificateError(LocalFrame*,
                                     const ResourceResponse&,
                                     network::mojom::RequestContextFrameType,
                                     mojom::RequestContextType);

  // Receive information about mixed content found externally.
  static void MixedContentFound(LocalFrame*,
                                const KURL& main_resource_url,
                                const KURL& mixed_content_url,
                                mojom::RequestContextType,
                                bool was_allowed,
                                bool had_redirect,
                                std::unique_ptr<SourceLocation>);

 private:
  FRIEND_TEST_ALL_PREFIXES(MixedContentCheckerTest, HandleCertificateError);

  static Frame* InWhichFrameIsContentMixed(
      Frame*,
      network::mojom::RequestContextFrameType,
      const KURL&,
      const LocalFrame*);

  static ConsoleMessage* CreateConsoleMessageAboutFetch(
      const KURL&,
      const KURL&,
      mojom::RequestContextType,
      bool allowed,
      std::unique_ptr<SourceLocation>);
  static ConsoleMessage* CreateConsoleMessageAboutWebSocket(const KURL&,
                                                            const KURL&,
                                                            bool allowed);
  static void Count(Frame*, mojom::RequestContextType, const LocalFrame*);

  DISALLOW_COPY_AND_ASSIGN(MixedContentChecker);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_MIXED_CONTENT_CHECKER_H_
