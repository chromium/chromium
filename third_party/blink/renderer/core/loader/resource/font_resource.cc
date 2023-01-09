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

#include "base/metrics/histogram_functions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/memory_dump_manager.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/mojom/loader/request_context_frame_type.mojom-blink.h"
#include "third_party/blink/renderer/platform/fonts/font_custom_platform_data.h"
#include "third_party/blink/renderer/platform/fonts/font_platform_data.h"
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
constexpr base::TimeDelta kFontLoadWaitShort = base::Milliseconds(100);
constexpr base::TimeDelta kFontLoadWaitLong = base::Milliseconds(3000);

FontResource* FontResource::Fetch(FetchParameters& params,
                                  ResourceFetcher* fetcher,
                                  FontResourceClient* client) {
  params.SetRequestContext(mojom::blink::RequestContextType::FONT);
  params.SetRequestDestination(network::mojom::RequestDestination::kFont);
  return To<FontResource>(
      fetcher->RequestResource(params, FontResourceFactory(), client));
}

FontResource::FontResource(const ResourceRequest& resource_request,
                           const ResourceLoaderOptions& options)
    : Resource(resource_request, ResourceType::kFont, options),
      load_limit_state_(LoadLimitState::kLoadNotStarted),
      cors_failed_(false) {}

FontResource::~FontResource() = default;

void FontResource::DidAddClient(ResourceClient* c) {
  DCHECK(c->IsFontResourceClient());
  Resource::DidAddClient(c);

  // Block client callbacks if currently loading from cache.
  if (IsLoading() && Loader()->IsCacheAwareLoadingActivated())
    return;

  ProhibitAddRemoveClientInScope prohibit_add_remove_client(this);
  if (load_limit_state_ == LoadLimitState::kShortLimitExceeded ||
      load_limit_state_ == LoadLimitState::kLongLimitExceeded)
    static_cast<FontResourceClient*>(c)->FontLoadShortLimitExceeded(this);
  if (load_limit_state_ == LoadLimitState::kLongLimitExceeded)
    static_cast<FontResourceClient*>(c)->FontLoadLongLimitExceeded(this);
}

void FontResource::SetRevalidatingRequest(const ResourceRequestHead& request) {
  // Reload will use the same object, and needs to reset |m_loadLimitState|
  // before any didAddClient() is called again.
  DCHECK(IsLoaded());
  DCHECK(!font_load_short_limit_.IsActive());
  DCHECK(!font_load_long_limit_.IsActive());
  load_limit_state_ = LoadLimitState::kLoadNotStarted;
  Resource::SetRevalidatingRequest(request);
}

void FontResource::StartLoadLimitTimersIfNecessary(
    base::SingleThreadTaskRunner* task_runner) {
  if (!IsLoading() || load_limit_state_ != LoadLimitState::kLoadNotStarted)
    return;
  DCHECK(!font_load_short_limit_.IsActive());
  DCHECK(!font_load_long_limit_.IsActive());
  load_limit_state_ = LoadLimitState::kUnderLimit;

  font_load_short_limit_ = PostDelayedCancellableTask(
      *task_runner, FROM_HERE,
      WTF::BindOnce(&FontResource::FontLoadShortLimitCallback,
                    WrapWeakPersistent(this)),
      kFontLoadWaitShort);
  font_load_long_limit_ = PostDelayedCancellableTask(
      *task_runner, FROM_HERE,
      WTF::BindOnce(&FontResource::FontLoadLongLimitCallback,
                    WrapWeakPersistent(this)),
      kFontLoadWaitLong);
}

scoped_refptr<FontCustomPlatformData> FontResource::GetCustomFontData() {
  if (!font_data_ && !ErrorOccurred() && !IsLoading()) {
    if (Data())
      font_data_ = FontCustomPlatformData::Create(Data(), ots_parsing_message_);

    if (!font_data_) {
      SetStatus(ResourceStatus::kDecodeError);
    } else {
      // Call observers once and remove them.
      HeapHashSet<WeakMember<FontResourceClearDataObserver>> observers;
      observers.swap(clear_data_observers_);
      for (const auto& observer : observers)
        observer->FontResourceDataWillBeCleared();
      ClearData();
    }
  }
  return font_data_;
}

void FontResource::WillReloadAfterDiskCacheMiss() {
  DCHECK(IsLoading());
  DCHECK(Loader()->IsCacheAwareLoadingActivated());
  if (load_limit_state_ == LoadLimitState::kShortLimitExceeded ||
      load_limit_state_ == LoadLimitState::kLongLimitExceeded) {
    NotifyClientsShortLimitExceeded();
  }
  if (load_limit_state_ == LoadLimitState::kLongLimitExceeded)
    NotifyClientsLongLimitExceeded();
}

void FontResource::FontLoadShortLimitCallback() {
  DCHECK(IsLoading());
  DCHECK_EQ(load_limit_state_, LoadLimitState::kUnderLimit);
  load_limit_state_ = LoadLimitState::kShortLimitExceeded;

  // Block client callbacks if currently loading from cache.
  if (Loader()->IsCacheAwareLoadingActivated())
    return;
  NotifyClientsShortLimitExceeded();
}

void FontResource::FontLoadLongLimitCallback() {
  DCHECK(IsLoading());
  DCHECK_EQ(load_limit_state_, LoadLimitState::kShortLimitExceeded);
  load_limit_state_ = LoadLimitState::kLongLimitExceeded;

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

  const char* system_allocator_name =
      base::trace_event::MemoryDumpManager::GetInstance()
          ->system_allocator_pool_name();
  if (system_allocator_name) {
    memory_dump->AddSuballocation(dump->Guid(), system_allocator_name);
  }
}

void FontResource::AddClearDataObserver(
    FontResourceClearDataObserver* observer) const {
  clear_data_observers_.insert(observer);
}

void FontResource::Trace(Visitor* visitor) const {
  visitor->Trace(clear_data_observers_);
  Resource::Trace(visitor);
}

}  // namespace blink
