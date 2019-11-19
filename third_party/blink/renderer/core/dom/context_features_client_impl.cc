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

#include "third_party/blink/renderer/core/dom/context_features_client_impl.h"

#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class ContextFeaturesCache final
    : public GarbageCollected<ContextFeaturesCache>,
      public Supplement<Document> {
  USING_GARBAGE_COLLECTED_MIXIN(ContextFeaturesCache);

 public:
  static const char kSupplementName[];

  class Entry {
    DISALLOW_NEW();

   public:
    enum Value { kIsEnabled, kIsDisabled, kNeedsRefresh };

    Entry() : value_(kNeedsRefresh), default_value_(false) {}

    bool IsEnabled() const {
      DCHECK_NE(value_, kNeedsRefresh);
      return value_ == kIsEnabled;
    }

    void Set(bool value, bool default_value) {
      value_ = value ? kIsEnabled : kIsDisabled;
      default_value_ = default_value;
    }

    bool NeedsRefresh(bool default_value) const {
      return value_ == kNeedsRefresh || default_value_ != default_value;
    }

   private:
    Value value_;
    bool default_value_;  // Needs to be traked as a part of the signature since
                          // it can be changed dynamically.
  };

  static ContextFeaturesCache& From(Document&);

  explicit ContextFeaturesCache(Document& document)
      : Supplement<Document>(document) {}

  Entry& EntryFor(ContextFeatures::FeatureType type) {
    size_t index = static_cast<size_t>(type);
    SECURITY_DCHECK(index < ContextFeatures::kFeatureTypeSize);
    return entries_[index];
  }

  void ValidateAgainst(Document*);

  void Trace(Visitor* visitor) override {
    Supplement<Document>::Trace(visitor);
  }

 private:
  String domain_;
  Entry entries_[ContextFeatures::kFeatureTypeSize];
};

const char ContextFeaturesCache::kSupplementName[] = "ContextFeaturesCache";

ContextFeaturesCache& ContextFeaturesCache::From(Document& document) {
  ContextFeaturesCache* cache =
      Supplement<Document>::From<ContextFeaturesCache>(document);
  if (!cache) {
    cache = MakeGarbageCollected<ContextFeaturesCache>(document);
    ProvideTo(document, cache);
  }

  return *cache;
}

void ContextFeaturesCache::ValidateAgainst(Document* document) {
  String current_domain = document->GetSecurityOrigin()->Domain();
  if (current_domain == domain_)
    return;
  domain_ = current_domain;
  for (size_t i = 0; i < ContextFeatures::kFeatureTypeSize; ++i)
    entries_[i] = Entry();
}

bool ContextFeaturesClientImpl::IsEnabled(Document* document,
                                          ContextFeatures::FeatureType type,
                                          bool default_value) {
  DCHECK(document);
  ContextFeaturesCache::Entry& cache =
      ContextFeaturesCache::From(*document).EntryFor(type);
  if (cache.NeedsRefresh(default_value))
    cache.Set(AskIfIsEnabled(document, type, default_value), default_value);
  return cache.IsEnabled();
}

void ContextFeaturesClientImpl::UrlDidChange(Document* document) {
  DCHECK(document);
  ContextFeaturesCache::From(*document).ValidateAgainst(document);
}

bool ContextFeaturesClientImpl::AskIfIsEnabled(
    Document* document,
    ContextFeatures::FeatureType type,
    bool default_value) {
  LocalFrame* frame = document->GetFrame();
  if (!frame || !frame->GetContentSettingsClient())
    return default_value;

  switch (type) {
    case ContextFeatures::kMutationEvents:
      return frame->GetContentSettingsClient()->AllowMutationEvents(
          default_value);
    default:
      return default_value;
  }
}

}  // namespace blink
