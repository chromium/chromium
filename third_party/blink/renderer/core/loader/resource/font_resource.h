/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_RESOURCE_FONT_RESOURCE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_RESOURCE_FONT_RESOURCE_H_

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/single_thread_task_runner.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_client.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cancellable_task.h"

namespace blink {

class FetchParameters;
class ResourceFetcher;
class FontCustomPlatformData;
class FontResourceClient;

class CORE_EXPORT FontResource final : public Resource {
 public:
  static FontResource* Fetch(FetchParameters&,
                             ResourceFetcher*,
                             FontResourceClient*);

  FontResource(const ResourceRequest&, const ResourceLoaderOptions&);
  ~FontResource() override;

  void DidAddClient(ResourceClient*) override;

  void SetRevalidatingRequest(const ResourceRequest&) override;

  void AllClientsAndObserversRemoved() override;
  void StartLoadLimitTimersIfNecessary(base::SingleThreadTaskRunner*);

  String OtsParsingMessage() const { return ots_parsing_message_; }

  scoped_refptr<FontCustomPlatformData> GetCustomFontData();

  // Returns true if the loading priority of the remote font resource can be
  // lowered. The loading priority of the font can be lowered only if the
  // font is not needed for painting the text.
  bool IsLowPriorityLoadingAllowedForRemoteFont() const;

  void WillReloadAfterDiskCacheMiss() override;

  void OnMemoryDump(WebMemoryDumpLevelOfDetail,
                    WebProcessMemoryDump*) const override;

 private:
  class FontResourceFactory : public NonTextResourceFactory {
   public:
    FontResourceFactory() : NonTextResourceFactory(ResourceType::kFont) {}

    Resource* Create(const ResourceRequest& request,
                     const ResourceLoaderOptions& options) const override {
      return MakeGarbageCollected<FontResource>(request, options);
    }
  };

  void NotifyFinished() override;
  void FontLoadShortLimitCallback();
  void FontLoadLongLimitCallback();
  void NotifyClientsShortLimitExceeded();
  void NotifyClientsLongLimitExceeded();

  // This is used in UMA histograms, should not change order.
  enum LoadLimitState {
    kLoadNotStarted,
    kUnderLimit,
    kShortLimitExceeded,
    kLongLimitExceeded,
    kLoadLimitStateEnumMax
  };

  scoped_refptr<FontCustomPlatformData> font_data_;
  String ots_parsing_message_;
  LoadLimitState load_limit_state_;
  bool cors_failed_;
  TaskHandle font_load_short_limit_;
  TaskHandle font_load_long_limit_;

  friend class MemoryCache;
  FRIEND_TEST_ALL_PREFIXES(CacheAwareFontResourceTest, CacheAwareFontLoading);
};

DEFINE_RESOURCE_TYPE_CASTS(Font);

class FontResourceClient : public ResourceClient {
 public:
  ~FontResourceClient() override = default;

  bool IsFontResourceClient() const final { return true; }

  // If cache-aware loading is activated, both callbacks will be blocked until
  // disk cache miss. Calls to addClient() and removeClient() in both callbacks
  // are prohibited to prevent race issues regarding current loading state.
  virtual void FontLoadShortLimitExceeded(FontResource*) {}
  virtual void FontLoadLongLimitExceeded(FontResource*) {}

  // Returns true if loading priority of remote font resources can be lowered.
  virtual bool IsLowPriorityLoadingAllowedForRemoteFont() const {
    // Only the RemoteFontFaceSources clients can prevent lowering of loading
    // priority of the remote fonts.  Set the default to true to prevent
    // other clients from incorrectly returning false.
    return true;
  }
};

}  // namespace blink

#endif
