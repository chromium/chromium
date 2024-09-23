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

#include <utility>

#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/mojom/loader/request_context_frame_type.mojom-blink.h"
#include "third_party/blink/renderer/platform/fonts/font_custom_platform_data.h"
#include "third_party/blink/renderer/platform/fonts/font_platform_data.h"
#include "third_party/blink/renderer/platform/fonts/web_font_decoder.h"
#include "third_party/blink/renderer/platform/heap/cross_thread_handle.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_client_walker.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/worker_pool.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_mojo.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"

#if BUILDFLAG(IS_WIN)
#include "third_party/blink/renderer/platform/scheduler/public/non_main_thread.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#endif  // IS_WIN

using ResultOrError =
    base::expected<blink::FontResource::DecodedResult, String>;

namespace WTF {

template <>
struct CrossThreadCopier<ResultOrError> {
  STATIC_ONLY(CrossThreadCopier);
  using Type = ResultOrError;
  static Type Copy(Type&& value) { return std::move(value); }
};

template <>
struct CrossThreadCopier<SegmentedBuffer> {
  STATIC_ONLY(CrossThreadCopier);
  using Type = SegmentedBuffer;
  static Type Copy(Type&& value) { return std::move(value); }
};

}  // namespace WTF

namespace blink {

namespace {
// Durations of font-display periods.
// https://tabatkins.github.io/specs/css-font-display/#font-display-desc
// TODO(toyoshim): Revisit short limit value once cache-aware font display is
// launched. crbug.com/570205
constexpr base::TimeDelta kFontLoadWaitShort = base::Milliseconds(100);
constexpr base::TimeDelta kFontLoadWaitLong = base::Milliseconds(3000);

base::expected<FontResource::DecodedResult, String> DecodeFont(
    SegmentedBuffer* buffer) {
  if (buffer->empty()) {
    // We don't have any data to decode. Just return an empty error string.
    return base::unexpected("");
  }
  WebFontDecoder decoder;
  auto decode_start_time = base::TimeTicks::Now();
  sk_sp<SkTypeface> typeface = decoder.Decode(buffer);
  base::UmaHistogramMicrosecondsTimes(
      "Blink.Fonts.BackgroundDecodeTime",
      base::TimeTicks::Now() - decode_start_time);
  if (typeface) {
    return FontResource::DecodedResult(std::move(typeface),
                                       decoder.DecodedSize());
  }
  return base::unexpected(decoder.GetErrorString());
}

scoped_refptr<base::SequencedTaskRunner> GetFontDecodingTaskRunner() {
#if BUILDFLAG(IS_WIN)
  // On Windows, the font decoding relies on FontManager, which requires
  // creating garbage collected objects. This means the thread the decoding
  // runs on must be GC enabled.
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      std::unique_ptr<NonMainThread>, font_decoding_thread,
      (NonMainThread::CreateThread(
          ThreadCreationParams(ThreadType::kFontThread).SetSupportsGC(true))));
  return font_decoding_thread->GetTaskRunner();
#else
  return worker_pool::CreateSequencedTaskRunner(
      {base::TaskPriority::USER_BLOCKING});
#endif  // IS_WIN
}

}  // namespace

class FontResource::BackgroundFontProcessor final
    : public BackgroundResponseProcessor,
      public mojo::DataPipeDrainer::Client {
 public:
  explicit BackgroundFontProcessor(
      CrossThreadWeakHandle<FontResource> resource_handle);
  ~BackgroundFontProcessor() override;

  BackgroundFontProcessor(const BackgroundFontProcessor&) = delete;
  BackgroundFontProcessor& operator=(const BackgroundFontProcessor&) = delete;

  // Implements BackgroundResponseProcessor interface.
  bool MaybeStartProcessingResponse(
      network::mojom::URLResponseHeadPtr& head,
      mojo::ScopedDataPipeConsumerHandle& body,
      std::optional<mojo_base::BigBuffer>& cached_metadata_buffer,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner,
      BackgroundResponseProcessor::Client* client) override;

  // Implements mojo::DataPipeDrainer::Client interface.
  void OnDataAvailable(base::span<const uint8_t> data) override;
  void OnDataComplete() override;

 private:
  static void DecodeOnBackgroundThread(
      SegmentedBuffer data,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner,
      base::WeakPtr<BackgroundFontProcessor> weak_this);

  void OnDecodeComplete(ResultOrError result_or_error, SegmentedBuffer data);

  network::mojom::URLResponseHeadPtr head_;
  std::optional<mojo_base::BigBuffer> cached_metadata_buffer_;
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;
  BackgroundResponseProcessor::Client* client_;

  std::unique_ptr<mojo::DataPipeDrainer> pipe_drainer_;
  SegmentedBuffer buffer_;
  CrossThreadWeakHandle<FontResource> resource_handle_;
  base::WeakPtrFactory<BackgroundFontProcessor> weak_factory_{this};
};

class FontResource::BackgroundFontProcessorFactory
    : public BackgroundResponseProcessorFactory {
 public:
  explicit BackgroundFontProcessorFactory(
      CrossThreadWeakHandle<FontResource> resource_handle);
  ~BackgroundFontProcessorFactory() override;
  BackgroundFontProcessorFactory(const BackgroundFontProcessorFactory&) =
      delete;
  BackgroundFontProcessorFactory& operator=(
      const BackgroundFontProcessorFactory&) = delete;
  std::unique_ptr<BackgroundResponseProcessor> Create() && override;

 private:
  CrossThreadWeakHandle<FontResource> resource_handle_;
};

FontResource::BackgroundFontProcessor::BackgroundFontProcessor(
    CrossThreadWeakHandle<FontResource> resource_handle)
    : resource_handle_(std::move(resource_handle)) {}

FontResource::BackgroundFontProcessor::~BackgroundFontProcessor() = default;

bool FontResource::BackgroundFontProcessor::MaybeStartProcessingResponse(
    network::mojom::URLResponseHeadPtr& head,
    mojo::ScopedDataPipeConsumerHandle& body,
    std::optional<mojo_base::BigBuffer>& cached_metadata_buffer,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner,
    BackgroundResponseProcessor::Client* client) {
  head_ = std::move(head);
  cached_metadata_buffer_ = std::move(cached_metadata_buffer);
  background_task_runner_ = background_task_runner;
  client_ = client;
  pipe_drainer_ =
      std::make_unique<mojo::DataPipeDrainer>(this, std::move(body));
  return true;
}

void FontResource::BackgroundFontProcessor::OnDataAvailable(
    base::span<const uint8_t> data) {
  buffer_.Append(Vector<char>(data));
}

void FontResource::BackgroundFontProcessor::OnDataComplete() {
  PostCrossThreadTask(
      *GetFontDecodingTaskRunner(), FROM_HERE,
      CrossThreadBindOnce(
          &FontResource::BackgroundFontProcessor::DecodeOnBackgroundThread,
          std::move(buffer_), background_task_runner_,
          weak_factory_.GetWeakPtr()));
}

// static
void FontResource::BackgroundFontProcessor::DecodeOnBackgroundThread(
    SegmentedBuffer data,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner,
    base::WeakPtr<BackgroundFontProcessor> weak_this) {
  base::expected<DecodedResult, String> result_or_error = DecodeFont(&data);
  PostCrossThreadTask(
      *background_task_runner, FROM_HERE,
      CrossThreadBindOnce(
          &FontResource::BackgroundFontProcessor::OnDecodeComplete,
          std::move(weak_this), std::move(result_or_error), std::move(data)));
}

void FontResource::BackgroundFontProcessor::OnDecodeComplete(
    base::expected<DecodedResult, String> result_or_error,
    SegmentedBuffer data) {
  DCHECK(background_task_runner_->RunsTasksInCurrentSequence());
  client_->PostTaskToMainThread(CrossThreadBindOnce(
      &FontResource::OnBackgroundDecodeFinished,
      MakeUnwrappingCrossThreadWeakHandle(std::move(resource_handle_)),
      std::move(result_or_error)));
  client_->DidFinishBackgroundResponseProcessor(
      std::move(head_), std::move(data), std::move(cached_metadata_buffer_));
}

FontResource::BackgroundFontProcessorFactory::BackgroundFontProcessorFactory(
    CrossThreadWeakHandle<FontResource> resource_handle)
    : resource_handle_(resource_handle) {}

FontResource::BackgroundFontProcessorFactory::
    ~BackgroundFontProcessorFactory() = default;

std::unique_ptr<BackgroundResponseProcessor>
FontResource::BackgroundFontProcessorFactory::Create() && {
  return std::make_unique<BackgroundFontProcessor>(std::move(resource_handle_));
}

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

const FontCustomPlatformData* FontResource::GetCustomFontData() {
  if (font_data_ || ErrorOccurred() || IsLoading()) {
    return font_data_;
  }
  if (Data()) {
    if (background_decode_result_or_error_) {
      if (background_decode_result_or_error_->has_value()) {
        font_data_ = FontCustomPlatformData::Create(
            std::move((*background_decode_result_or_error_)->sk_typeface),
            (*background_decode_result_or_error_)->decoded_size);
      } else {
        ots_parsing_message_ = background_decode_result_or_error_->error();
      }
    } else {
      auto decode_start_time = base::TimeTicks::Now();
      font_data_ = FontCustomPlatformData::Create(Data(), ots_parsing_message_);
      base::UmaHistogramMicrosecondsTimes(
          "Blink.Fonts.DecodeTime", base::TimeTicks::Now() - decode_start_time);
    }
  }

  if (!font_data_) {
    SetStatus(ResourceStatus::kDecodeError);
  } else {
    // Call observers once and remove them.
    HeapHashSet<WeakMember<FontResourceClearDataObserver>> observers;
    observers.swap(clear_data_observers_);
    for (const auto& observer : observers) {
      observer->FontResourceDataWillBeCleared();
    }
    ClearData();
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

std::unique_ptr<BackgroundResponseProcessorFactory>
FontResource::MaybeCreateBackgroundResponseProcessorFactory() {
  if (!features::kBackgroundFontResponseProcessor.Get()) {
    return nullptr;
  }
  return std::make_unique<BackgroundFontProcessorFactory>(
      MakeCrossThreadWeakHandle(this));
}

void FontResource::OnBackgroundDecodeFinished(
    base::expected<DecodedResult, String> result_or_error) {
  background_decode_result_or_error_ = std::move(result_or_error);
}

void FontResource::Trace(Visitor* visitor) const {
  visitor->Trace(font_data_);
  visitor->Trace(clear_data_observers_);
  Resource::Trace(visitor);
}

}  // namespace blink
