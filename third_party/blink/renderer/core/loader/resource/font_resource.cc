/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Torch Mobile, Inc.
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

#include "third_party/blink/renderer/core/loader/resource/font_resource.h"

#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/mojom/loader/request_context_frame_type.mojom-blink.h"
#include "third_party/blink/renderer/platform/fonts/font_custom_platform_data.h"
#include "third_party/blink/renderer/platform/fonts/font_platform_data.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_client_walker.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"

namespace blink {

// Durations of font-display periods.
// https://tabatkins.github.io/specs/css-font-display/#font-display-desc
// TODO(toyoshim): Revisit short limit value once cache-aware font display is
// launched. crbug.com/570205
constexpr base::TimeDelta kFontLoadWaitShort =
    base::TimeDelta::FromMilliseconds(100);
constexpr base::TimeDelta kFontLoadWaitLong =
    base::TimeDelta::FromMilliseconds(3000);

enum FontPackageFormat {
  kPackageFormatUnknown,
  kPackageFormatSFNT,
  kPackageFormatWOFF,
  kPackageFormatWOFF2,
  kPackageFormatSVG,
  kPackageFormatEnumMax
};

static FontPackageFormat PackageFormatOf(SharedBuffer* buffer) {
  static constexpr size_t kMaxHeaderSize = 4;
  char data[kMaxHeaderSize];
  if (!buffer->GetBytes(data, kMaxHeaderSize))
    return kPackageFormatUnknown;

  if (data[0] == 'w' && data[1] == 'O' && data[2] == 'F' && data[3] == 'F')
    return kPackageFormatWOFF;
  if (data[0] == 'w' && data[1] == 'O' && data[2] == 'F' && data[3] == '2')
    return kPackageFormatWOFF2;
  return kPackageFormatSFNT;
}

static void RecordPackageFormatHistogram(FontPackageFormat format) {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      EnumerationHistogram, package_format_histogram,
      ("WebFont.PackageFormat", kPackageFormatEnumMax));
  package_format_histogram.Count(format);
}

FontResource* FontResource::Fetch(FetchParameters& params,
                                  ResourceFetcher* fetcher,
                                  FontResourceClient* client) {
  params.SetRequestContext(mojom::RequestContextType::FONT);
  return ToFontResource(
      fetcher->RequestResource(params, FontResourceFactory(), client));
}

FontResource::FontResource(const ResourceRequest& resource_request,
                           const ResourceLoaderOptions& options)
    : Resource(resource_request, ResourceType::kFont, options),
      load_limit_state_(kLoadNotStarted),
      cors_failed_(false) {}

FontResource::~FontResource() = default;

void FontResource::DidAddClient(ResourceClient* c) {
  DCHECK(c->IsFontResourceClient());
  Resource::DidAddClient(c);

  // Block client callbacks if currently loading from cache.
  if (IsLoading() && Loader()->IsCacheAwareLoadingActivated())
    return;

  ProhibitAddRemoveClientInScope prohibit_add_remove_client(this);
  if (load_limit_state_ == kShortLimitExceeded ||
      load_limit_state_ == kLongLimitExceeded)
    static_cast<FontResourceClient*>(c)->FontLoadShortLimitExceeded(this);
  if (load_limit_state_ == kLongLimitExceeded)
    static_cast<FontResourceClient*>(c)->FontLoadLongLimitExceeded(this);
}

void FontResource::SetRevalidatingRequest(const ResourceRequest& request) {
  // Reload will use the same object, and needs to reset |m_loadLimitState|
  // before any didAddClient() is called again.
  DCHECK(IsLoaded());
  DCHECK(!font_load_short_limit_.IsActive());
  DCHECK(!font_load_long_limit_.IsActive());
  load_limit_state_ = kLoadNotStarted;
  Resource::SetRevalidatingRequest(request);
}

void FontResource::StartLoadLimitTimersIfNecessary(
    base::SingleThreadTaskRunner* task_runner) {
  if (!IsLoading() || load_limit_state_ != kLoadNotStarted)
    return;
  DCHECK(!font_load_short_limit_.IsActive());
  DCHECK(!font_load_long_limit_.IsActive());
  load_limit_state_ = kUnderLimit;

  font_load_short_limit_ = PostDelayedCancellableTask(
      *task_runner, FROM_HERE,
      WTF::Bind(&FontResource::FontLoadShortLimitCallback,
                WrapWeakPersistent(this)),
      kFontLoadWaitShort);
  font_load_long_limit_ = PostDelayedCancellableTask(
      *task_runner, FROM_HERE,
      WTF::Bind(&FontResource::FontLoadLongLimitCallback,
                WrapWeakPersistent(this)),
      kFontLoadWaitLong);
}

scoped_refptr<FontCustomPlatformData> FontResource::GetCustomFontData() {
  if (!font_data_ && !ErrorOccurred() && !IsLoading()) {
    if (Data())
      font_data_ = FontCustomPlatformData::Create(Data(), ots_parsing_message_);

    if (font_data_) {
      RecordPackageFormatHistogram(PackageFormatOf(Data()));
    } else {
      SetStatus(ResourceStatus::kDecodeError);
      RecordPackageFormatHistogram(kPackageFormatUnknown);
    }
  }
  return font_data_;
}

void FontResource::WillReloadAfterDiskCacheMiss() {
  DCHECK(IsLoading());
  DCHECK(Loader()->IsCacheAwareLoadingActivated());
  if (load_limit_state_ == kShortLimitExceeded ||
      load_limit_state_ == kLongLimitExceeded) {
    NotifyClientsShortLimitExceeded();
  }
  if (load_limit_state_ == kLongLimitExceeded)
    NotifyClientsLongLimitExceeded();

  DEFINE_STATIC_LOCAL(
      EnumerationHistogram, load_limit_histogram,
      ("WebFont.LoadLimitOnDiskCacheMiss", kLoadLimitStateEnumMax));
  load_limit_histogram.Count(load_limit_state_);
}

void FontResource::FontLoadShortLimitCallback() {
  DCHECK(IsLoading());
  DCHECK_EQ(load_limit_state_, kUnderLimit);
  load_limit_state_ = kShortLimitExceeded;

  // Block client callbacks if currently loading from cache.
  if (Loader()->IsCacheAwareLoadingActivated())
    return;
  NotifyClientsShortLimitExceeded();
}

void FontResource::FontLoadLongLimitCallback() {
  DCHECK(IsLoading());
  DCHECK_EQ(load_limit_state_, kShortLimitExceeded);
  load_limit_state_ = kLongLimitExceeded;

  // Block client callbacks if currently loading from cache.
  if (Loader()->IsCacheAwareLoadingActivated())
    return;
  NotifyClientsLongLimitExceeded();
}

void FontResource::NotifyClientsShortLimitExceeded() {
  ProhibitAddRemoveClientInScope prohibit_add_remove_client(this);
  ResourceClientWalker<FontResourceClient> walker(Clients());
  while (FontResourceClient* client = walker.Next())
    client->FontLoadShortLimitExceeded(this);
}

void FontResource::NotifyClientsLongLimitExceeded() {
  ProhibitAddRemoveClientInScope prohibit_add_remove_client(this);
  ResourceClientWalker<FontResourceClient> walker(Clients());
  while (FontResourceClient* client = walker.Next())
    client->FontLoadLongLimitExceeded(this);
}

void FontResource::AllClientsAndObserversRemoved() {
  font_data_ = nullptr;
  Resource::AllClientsAndObserversRemoved();
}

void FontResource::NotifyFinished() {
  font_load_short_limit_.Cancel();
  font_load_long_limit_.Cancel();

  Resource::NotifyFinished();
}

bool FontResource::IsLowPriorityLoadingAllowedForRemoteFont() const {
  DCHECK(!IsLoaded());
  if (Url().ProtocolIsData())
    return false;
  ResourceClientWalker<FontResourceClient> walker(Clients());
  while (FontResourceClient* client = walker.Next()) {
    if (!client->IsLowPriorityLoadingAllowedForRemoteFont()) {
      return false;
    }
  }
  return true;
}

void FontResource::OnMemoryDump(WebMemoryDumpLevelOfDetail level,
                                WebProcessMemoryDump* memory_dump) const {
  Resource::OnMemoryDump(level, memory_dump);
  if (!font_data_)
    return;
  const String name = GetMemoryDumpName() + "/decoded_webfont";
  WebMemoryAllocatorDump* dump = memory_dump->CreateMemoryAllocatorDump(name);
  dump->AddScalar("size", "bytes", font_data_->DataSize());
  memory_dump->AddSuballocation(dump->Guid(), "malloc");
}

}  // namespace blink
