/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_CONTEXT_FEATURES_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_CONTEXT_FEATURES_H_

#include <memory>
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class ContextFeaturesClient;
class Document;
class Page;

class ContextFeatures final : public GarbageCollected<ContextFeatures>,
                              public Supplement<Page> {
 public:
  static const char kSupplementName[];

  enum FeatureType {
    kPagePopup = 0,
    kMutationEvents,
    kFeatureTypeSize  // Should be the last entry.
  };

  static ContextFeatures& DefaultSwitch();

  static bool PagePopupEnabled(Document*);
  static bool MutationEventsEnabled(Document*);

  explicit ContextFeatures(std::unique_ptr<ContextFeaturesClient> client)
      : Supplement(nullptr), client_(std::move(client)) {}

  bool IsEnabled(Document*, FeatureType, bool) const;
  void UrlDidChange(Document*);

 private:
  std::unique_ptr<ContextFeaturesClient> client_;
};

class ContextFeaturesClient {
  USING_FAST_MALLOC(ContextFeaturesClient);

 public:
  static std::unique_ptr<ContextFeaturesClient> Empty();

  virtual ~ContextFeaturesClient() = default;
  virtual bool IsEnabled(Document*,
                         ContextFeatures::FeatureType,
                         bool default_value) {
    return default_value;
  }
  virtual void UrlDidChange(Document*) {}
};

CORE_EXPORT void ProvideContextFeaturesTo(
    Page&,
    std::unique_ptr<ContextFeaturesClient>);
void ProvideContextFeaturesToDocumentFrom(Document&, Page&);

inline bool ContextFeatures::IsEnabled(Document* document,
                                       FeatureType type,
                                       bool default_value) const {
  if (!client_)
    return default_value;
  return client_->IsEnabled(document, type, default_value);
}

inline void ContextFeatures::UrlDidChange(Document* document) {
  // FIXME: The original code, commented out below, is obviously
  // wrong, but the seemingly correct fix of negating the test to
  // the more logical 'if (!client_)' crashes the renderer.
  // See issue 294180
  //
  // if (client_)
  //   return;
  // client_->UrlDidChange(document);
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_CONTEXT_FEATURES_H_
