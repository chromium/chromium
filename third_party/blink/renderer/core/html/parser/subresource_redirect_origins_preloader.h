// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_SUBRESOURCE_REDIRECT_ORIGINS_PRELOADER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_SUBRESOURCE_REDIRECT_ORIGINS_PRELOADER_H_

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin_hash.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// Helper class for preloading subresource redirect optimizations for images.
// Tied with the lifetime of a Document.
class SubresourceRedirectOriginsPreloader final
    : public GarbageCollected<SubresourceRedirectOriginsPreloader>,
      public Supplement<Document> {
 public:
  static const char kSupplementName[];

  explicit SubresourceRedirectOriginsPreloader(Document&);
  SubresourceRedirectOriginsPreloader(
      const SubresourceRedirectOriginsPreloader&) = delete;
  SubresourceRedirectOriginsPreloader& operator=(
      const SubresourceRedirectOriginsPreloader&) = delete;
  virtual ~SubresourceRedirectOriginsPreloader() = default;

  static SubresourceRedirectOriginsPreloader* From(Document&);

  // Adds the origin computed from |base_url| and |resource_url| to origins that
  // require preloading optimizations. |resource_url| is the URL of the
  // resource, and origin is computed from it along with |base_url|. When
  // |resource_url| is absolute, |base_url| will not be used. When
  // |resource_url| is relative, |base_url| determines the base origin. For
  // relative |resource_url| when |base_url| is empty, the document's base URL
  // will be used.
  void AddImagePreloadRequest(const KURL& base_url, const String& resource_url);

  // Triggers preloading the optimizations.
  void PreloadOriginsNow();

  void Trace(Visitor*) const override;

 private:
  // The origins for which subresource redirect optimizations should be
  // preloaded.
  WTF::HashSet<scoped_refptr<const SecurityOrigin>, SecurityOriginHash>
      origins_;
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_SUBRESOURCE_REDIRECT_ORIGINS_PRELOADER_H_
