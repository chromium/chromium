// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_ANCHOR_ELEMENT_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_ANCHOR_ELEMENT_UTILS_H_

#include <cstdint>

#include "third_party/blink/renderer/core/core_export.h"

namespace blink {

// Link relation bitmask values.
// FIXME: Uncomment as the various link relations are implemented.
enum {
  //     RelationAlternate   = 0x00000001,
  //     RelationArchives    = 0x00000002,
  //     RelationAuthor      = 0x00000004,
  //     RelationBookmark     = 0x00000008,
  //     RelationExternal    = 0x00000010,
  //     RelationFirst       = 0x00000020,
  //     RelationHelp        = 0x00000040,
  //     RelationIndex       = 0x00000080,
  //     RelationLast        = 0x00000100,
  //     RelationLicense     = 0x00000200,
  //     RelationNext        = 0x00000400,
  //     RelationNoFollow    = 0x00000800,
  kRelationNoReferrer = 0x00001000,
  //     RelationPrev        = 0x00002000,
  //     RelationSearch      = 0x00004000,
  //     RelationSidebar     = 0x00008000,
  //     RelationTag         = 0x00010000,
  //     RelationUp          = 0x00020000,
  kRelationNoOpener = 0x00040000,
  kRelationOpener = 0x00080000,
  kRelationPrivacyPolicy = 0x00100000,
  kRelationTermsOfService = 0x00200000,
};

class AtomicString;
class Document;
class Element;
class ExecutionContext;
struct FrameLoadRequest;
class LocalDOMWindow;
class ResourceRequest;
class Settings;
class String;
class KURL;

class CORE_EXPORT AnchorElementUtils {
 public:
  static void HandleDownloadAttribute(Element* element,
                                      const String& download_attr,
                                      const KURL& url,
                                      LocalDOMWindow* window,
                                      bool is_trusted,
                                      ResourceRequest request);

  // Process `rel` attribute values (`noopener`, `noreferrer`, etc.) and
  // apply corresponding security policies to the navigation request.
  static void HandleRelAttribute(FrameLoadRequest& frame_request,
                                 const Settings* settings,
                                 ExecutionContext* execution_context,
                                 const AtomicString& target,
                                 uint32_t link_relations);
  // TODO: Replace uint32_t with base::EnumSet, if possible.
  static bool HasRel(uint32_t link_relations, uint32_t relation);
  static uint32_t ParseRelAttribute(const AtomicString& value,
                                    Document& document);

  static void SendPings(const KURL& destination_url,
                        Document& document,
                        const AtomicString& ping_value);

  static void HandleReferrerPolicyAttribute(ResourceRequest& request,
                                            const AtomicString& referrer_policy,
                                            uint32_t link_relations,
                                            Document& document);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_ANCHOR_ELEMENT_UTILS_H_
