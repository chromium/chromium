/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Waldo Bastian (bastian@kde.org)
    Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
    Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.

    This class provides all functionality needed for loading images, style
    sheets and html pages from the web. It has a memory cache for these objects.
*/

#include "third_party/blink/renderer/core/loader/resource/css_style_sheet_resource.h"

#include "base/trace_event/trace_event.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/mojom/loader/request_context_frame_type.mojom-blink.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/fetch/response_body_loader.h"
#include "third_party/blink/renderer/platform/loader/fetch/text_resource_decoder_options.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/network/mime/mime_type_registry.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/worker_pool.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_mojo.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_std.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"

namespace blink {
namespace {

class CSSDecodedDataInfo final : public Resource::DecodedDataInfo {
 public:
  CSSDecodedDataInfo(std::unique_ptr<CachedCSSTokenizer> tokenizer,
                     const String& encoding)
      : tokenizer_(std::move(tokenizer)), encoding_(encoding) {}

  ResourceType GetType() const override { return ResourceType::kCSSStyleSheet; }

  std::unique_ptr<CachedCSSTokenizer> tokenizer_;
  String encoding_;
};

}  // namespace

template <>
struct DowncastTraits<CSSDecodedDataInfo> {
  static bool AllowFrom(const Resource::DecodedDataInfo& info) {
    return info.GetType() == ResourceType::kCSSStyleSheet;
  }
};

class CSSStyleSheetResource::CSSTokenizerWorker final {
 public:
  CSSTokenizerWorker(
      mojo::ScopedDataPipeConsumerHandle data_pipe,
      ResponseBodyLoaderClient* response_body_loader_client,
      std::unique_ptr<TextResourceDecoder> decoder,
      scoped_refptr<base::SingleThreadTaskRunner> loader_task_runner)
      : data_pipe_(std::move(data_pipe)),
        response_body_loader_client_(response_body_loader_client),
        decoder_(std::move(decoder)),
        loader_task_runner_(std::move(loader_task_runner)) {
    watcher_ = std::make_unique<mojo::SimpleWatcher>(
        FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL);

    watcher_->Watch(data_pipe_.get(), MOJO_HANDLE_SIGNAL_READABLE,
                    MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
                    WTF::BindRepeating(&CSSTokenizerWorker::OnDataPipeReadable,
                                       WTF::Unretained(this)));

    MojoResult ready_result;
    mojo::HandleSignalsState ready_state;
    MojoResult rv = watcher_->Arm(&ready_result, &ready_state);
    if (rv == MOJO_RESULT_OK)
      return;

    DCHECK_EQ(MOJO_RESULT_FAILED_PRECONDITION, rv);
    OnDataPipeReadable(ready_result, ready_state);
  }

  void OnDataPipeReadable(MojoResult result,
                          const mojo::HandleSignalsState& state) {
    // A non-OK result means we've read all the data or there is an error.
    if (result != MOJO_RESULT_OK) {
      TRACE_EVENT0("blink", "CSSTokenizerWorker::Tokenize");
      watcher_.reset();

      std::unique_ptr<CSSDecodedDataInfo> info;
      String text;
      // This means the load succeeded. If no data has been received, the text
      // should be null.
      if (result == MOJO_RESULT_FAILED_PRECONDITION && has_data_) {
        builder_.Append(decoder_->Flush());
        text = builder_.ReleaseString();
        // The expensive tokenization work we want to complete in the background
        // is done in the CreateCachedTokenizer() call below.
        info = std::make_unique<CSSDecodedDataInfo>(
            CSSTokenizer::CreateCachedTokenizer(text),
            String(decoder_->Encoding().GetName()));
      }
      PostCrossThreadTask(*loader_task_runner_, FROM_HERE,
                          CrossThreadBindOnce(NotifyClientDidFinishLoading,
                                              response_body_loader_client_,
                                              text, std::move(info), result));
      return;
    }

    CHECK(state.readable());
    CHECK(data_pipe_);

    const void* data;
    uint32_t data_size;
    // There should be data, so this read should succeed.
    CHECK_EQ(
        data_pipe_->BeginReadData(&data, &data_size, MOJO_READ_DATA_FLAG_NONE),
        MOJO_RESULT_OK);
    has_data_ = true;

    auto copy_for_resource = std::make_unique<char[]>(data_size);
    memcpy(copy_for_resource.get(), data, data_size);
    PostCrossThreadTask(
        *loader_task_runner_, FROM_HERE,
        CrossThreadBindOnce(NotifyClientDidReceiveData,
                            response_body_loader_client_,
                            std::move(copy_for_resource), data_size));
    builder_.Append(
        decoder_->Decode(reinterpret_cast<const char*>(data), data_size));

    CHECK_EQ(data_pipe_->EndReadData(data_size), MOJO_RESULT_OK);

    watcher_->ArmOrNotify();
  }

 private:
  static void NotifyClientDidReceiveData(
      ResponseBodyLoaderClient* response_body_loader_client,
      std::unique_ptr<char[]> data,
      size_t data_size) {
    DCHECK(IsMainThread());
    // The response_body_loader_client is held weakly, so it may be dead by the
    // time this callback is called. If so, we can simply drop this chunk.
    if (!response_body_loader_client)
      return;

    response_body_loader_client->DidReceiveData(
        base::make_span(data.get(), data_size));
  }

  static void NotifyClientDidFinishLoading(
      ResponseBodyLoaderClient* response_body_loader_client,
      const String& decoded_sheet_text,
      std::unique_ptr<CSSDecodedDataInfo> info,
      MojoResult result) {
    DCHECK(IsMainThread());
    if (!response_body_loader_client)
      return;

    switch (result) {
      case MOJO_RESULT_CANCELLED:
        response_body_loader_client->DidCancelLoadingBody();
        break;
      case MOJO_RESULT_FAILED_PRECONDITION:
        response_body_loader_client->DidReceiveDecodedData(decoded_sheet_text,
                                                           std::move(info));
        response_body_loader_client->DidFinishLoadingBody();
        break;
      default:
        response_body_loader_client->DidFailLoadingBody();
        break;
    }
  }

  std::unique_ptr<mojo::SimpleWatcher> watcher_;
  mojo::ScopedDataPipeConsumerHandle data_pipe_;
  CrossThreadWeakPersistent<ResponseBodyLoaderClient>
      response_body_loader_client_;
  std::unique_ptr<TextResourceDecoder> decoder_;
  scoped_refptr<base::SingleThreadTaskRunner> loader_task_runner_;
  bool has_data_ = false;

  StringBuilder builder_;
};

CSSStyleSheetResource* CSSStyleSheetResource::Fetch(FetchParameters& params,
                                                    ResourceFetcher* fetcher,
                                                    ResourceClient* client) {
  params.SetRequestContext(mojom::blink::RequestContextType::STYLE);
  params.SetRequestDestination(network::mojom::RequestDestination::kStyle);
  auto* resource = To<CSSStyleSheetResource>(
      fetcher->RequestResource(params, CSSStyleSheetResourceFactory(), client));
  return resource;
}

CSSStyleSheetResource* CSSStyleSheetResource::CreateForTest(
    const KURL& url,
    const WTF::TextEncoding& encoding) {
  ResourceRequest request(url);
  request.SetCredentialsMode(network::mojom::CredentialsMode::kOmit);
  ResourceLoaderOptions options(nullptr /* world */);
  TextResourceDecoderOptions decoder_options(
      TextResourceDecoderOptions::kCSSContent, encoding);
  return MakeGarbageCollected<CSSStyleSheetResource>(request, options,
                                                     decoder_options);
}

CSSStyleSheetResource::CSSStyleSheetResource(
    const ResourceRequest& resource_request,
    const ResourceLoaderOptions& options,
    const TextResourceDecoderOptions& decoder_options)
    : TextResource(resource_request,
                   ResourceType::kCSSStyleSheet,
                   options,
                   decoder_options),
      tokenizer_text_decoder_(
          std::make_unique<TextResourceDecoder>(decoder_options)) {}

CSSStyleSheetResource::~CSSStyleSheetResource() = default;

void CSSStyleSheetResource::SetParsedStyleSheetCache(
    StyleSheetContents* new_sheet) {
  if (parsed_style_sheet_cache_)
    parsed_style_sheet_cache_->ClearReferencedFromResource();
  parsed_style_sheet_cache_ = new_sheet;
  if (parsed_style_sheet_cache_)
    parsed_style_sheet_cache_->SetReferencedFromResource(this);

  // Updates the decoded size to take parsed stylesheet cache into account.
  UpdateDecodedSize();
}

void CSSStyleSheetResource::Trace(Visitor* visitor) const {
  visitor->Trace(parsed_style_sheet_cache_);
  TextResource::Trace(visitor);
}

void CSSStyleSheetResource::OnMemoryDump(
    WebMemoryDumpLevelOfDetail level_of_detail,
    WebProcessMemoryDump* memory_dump) const {
  Resource::OnMemoryDump(level_of_detail, memory_dump);
  const String name = GetMemoryDumpName() + "/style_sheets";
  auto* dump = memory_dump->CreateMemoryAllocatorDump(name);
  dump->AddScalar("size", "bytes", decoded_sheet_text_.CharactersSizeInBytes());
  memory_dump->AddSuballocation(
      dump->Guid(), String(WTF::Partitions::kAllocatedObjectPoolName));
}

void CSSStyleSheetResource::SetEncoding(const String& chs) {
  TextResource::SetEncoding(chs);
  if (tokenizer_text_decoder_) {
    tokenizer_text_decoder_->SetEncoding(
        WTF::TextEncoding(chs), TextResourceDecoder::kEncodingFromHTTPHeader);
  }
}

void CSSStyleSheetResource::ResponseBodyReceived(
    ResponseBodyLoaderDrainableInterface& body_loader,
    scoped_refptr<base::SingleThreadTaskRunner> loader_task_runner) {
  if (!base::FeatureList::IsEnabled(features::kPretokenizeCSS) ||
      !features::kPretokenizeExternalSheets.Get() || !tokenizer_text_decoder_) {
    return;
  }

  ResponseBodyLoaderClient* response_body_loader_client = nullptr;
  mojo::ScopedDataPipeConsumerHandle data_pipe =
      body_loader.DrainAsDataPipe(&response_body_loader_client);
  if (!data_pipe)
    return;

  AdvanceLoadingState(LoadingState::kTokenizing);
  worker_ = WTF::SequenceBound<CSSTokenizerWorker>(
      worker_pool::CreateSequencedTaskRunner(
          {base::TaskPriority::USER_BLOCKING}),
      std::move(data_pipe),
      WrapCrossThreadWeakPersistent(response_body_loader_client),
      std::move(tokenizer_text_decoder_), loader_task_runner);
}

void CSSStyleSheetResource::DidReceiveDecodedData(
    const String& data,
    std::unique_ptr<DecodedDataInfo> info) {
  CHECK_EQ(loading_state_, LoadingState::kTokenizing);
  SetDecodedSheetText(data);
  if (!info)
    return;

  auto* css_info = To<CSSDecodedDataInfo>(info.get());
  tokenizer_ = std::move(css_info->tokenizer_);

  // The encoding may have been autodetected when decoding the data, so make
  // sure to set the final encoding here. Calling TextResource::SetEncoding is
  // still a little inconsistent (e.g. EncodingSource is not set properly, the
  // autodetected encoding is set while the decoder itself is not used) but this
  // should be OK for now as TextResource::decoder_ is used only for Encoding()
  // after this point.
  // TODO: Clean this up if needed.
  TextResource::SetEncoding(css_info->encoding_);
}

network::mojom::ReferrerPolicy CSSStyleSheetResource::GetReferrerPolicy()
    const {
  network::mojom::ReferrerPolicy referrer_policy =
      network::mojom::ReferrerPolicy::kDefault;
  String referrer_policy_header =
      GetResponse().HttpHeaderField(http_names::kReferrerPolicy);
  if (!referrer_policy_header.IsNull()) {
    SecurityPolicy::ReferrerPolicyFromHeaderValue(
        referrer_policy_header, kDoNotSupportReferrerPolicyLegacyKeywords,
        &referrer_policy);
  }
  return referrer_policy;
}

const String CSSStyleSheetResource::SheetText(
    const CSSParserContext* parser_context,
    MIMETypeCheck mime_type_check) const {
  if (!CanUseSheet(parser_context, mime_type_check))
    return String();

  // Use cached decoded sheet text when available
  if (!decoded_sheet_text_.IsNull()) {
    // We should have the decoded sheet text cached when the resource is fully
    // loaded.
    DCHECK_EQ(GetStatus(), ResourceStatus::kCached);

    return decoded_sheet_text_;
  }

  if (!Data() || Data()->empty())
    return String();

  return DecodedText();
}

void CSSStyleSheetResource::NotifyFinished() {
  // The worker has completed decoding and tokenization so is no longer needed.
  worker_.Reset();

  // Decode the data to find out the encoding and cache the decoded sheet text.
  switch (loading_state_) {
    case LoadingState::kTokenizing:
      if (Data()) {
        // If tokenizing has already happened the decoded sheet text will
        // already be set.
        DCHECK(!decoded_sheet_text_.IsNull());
      } else {
        DCHECK(LoadFailedOrCanceled() || decoded_sheet_text_.IsNull());
      }
      break;
    case LoadingState::kLoading:
      if (Data()) {
        DCHECK(decoded_sheet_text_.IsNull());
        SetDecodedSheetText(DecodedText());
      }
      break;
    case LoadingState::kFinished:
      NOTREACHED();
      break;
  }

  AdvanceLoadingState(LoadingState::kFinished);

  Resource::NotifyFinished();

  // Clear raw bytes as now we have the full decoded sheet text.
  // We wait for all LinkStyle::setCSSStyleSheet to run (at least once)
  // as SubresourceIntegrity checks require raw bytes.
  // Note that LinkStyle::setCSSStyleSheet can be called from didAddClient too,
  // but is safe as we should have a cached ResourceIntegrityDisposition.
  ClearData();
}

void CSSStyleSheetResource::DestroyDecodedDataIfPossible() {
  tokenizer_.reset();
  if (!parsed_style_sheet_cache_)
    return;

  SetParsedStyleSheetCache(nullptr);
}

void CSSStyleSheetResource::DestroyDecodedDataForFailedRevalidation() {
  SetDecodedSheetText(String());
  DestroyDecodedDataIfPossible();
}

void CSSStyleSheetResource::SetRevalidatingRequest(
    const ResourceRequestHead& head) {
  TextResource::SetRevalidatingRequest(head);
  AdvanceLoadingState(LoadingState::kLoading);
}

bool CSSStyleSheetResource::CanUseSheet(const CSSParserContext* parser_context,
                                        MIMETypeCheck mime_type_check) const {
  if (ErrorOccurred())
    return false;

  // For `file:` URLs, we may need to be a little more strict than the below.
  // Though we'll likely change this in the future, for the moment we're going
  // to enforce a file-extension requirement on stylesheets loaded from `file:`
  // URLs and see how far it gets us.
  KURL sheet_url = GetResponse().CurrentRequestUrl();
  if (sheet_url.IsLocalFile()) {
    if (parser_context) {
      parser_context->Count(WebFeature::kLocalCSSFile);
    }
    // Grab |sheet_url|'s filename's extension (if present), and check whether
    // or not it maps to a `text/css` MIME type:
    String extension;
    int last_dot = sheet_url.LastPathComponent().ReverseFind('.');
    if (last_dot != -1)
      extension = sheet_url.LastPathComponent().Substring(last_dot + 1);
    if (!EqualIgnoringASCIICase(
            MIMETypeRegistry::GetMIMETypeForExtension(extension), "text/css")) {
      if (parser_context) {
        parser_context->CountDeprecation(
            WebFeature::kLocalCSSFileExtensionRejected);
      }
      return false;
    }
  }

  // This check exactly matches Firefox. Note that we grab the Content-Type
  // header directly because we want to see what the value is BEFORE content
  // sniffing. Firefox does this by setting a "type hint" on the channel. This
  // implementation should be observationally equivalent.
  //
  // This code defaults to allowing the stylesheet for non-HTTP protocols so
  // folks can use standards mode for local HTML documents.
  if (mime_type_check == MIMETypeCheck::kLax)
    return true;
  AtomicString content_type = HttpContentType();
  return content_type.empty() ||
         EqualIgnoringASCIICase(content_type, "text/css") ||
         EqualIgnoringASCIICase(content_type,
                                "application/x-unknown-content-type");
}

StyleSheetContents* CSSStyleSheetResource::CreateParsedStyleSheetFromCache(
    const CSSParserContext* context) {
  if (!parsed_style_sheet_cache_)
    return nullptr;
  if (parsed_style_sheet_cache_->HasFailedOrCanceledSubresources()) {
    SetParsedStyleSheetCache(nullptr);
    return nullptr;
  }

  DCHECK(parsed_style_sheet_cache_->IsCacheableForResource());
  DCHECK(parsed_style_sheet_cache_->IsReferencedFromResource());

  // Contexts must be identical so we know we would get the same exact result if
  // we parsed again.
  if (*parsed_style_sheet_cache_->ParserContext() != *context)
    return nullptr;

  DCHECK(!parsed_style_sheet_cache_->IsLoading());

  // If the stylesheet has a media query, we need to clone the cached sheet
  // due to potential differences in the rule set.
  if (parsed_style_sheet_cache_->HasMediaQueries())
    return parsed_style_sheet_cache_->Copy();

  return parsed_style_sheet_cache_;
}

void CSSStyleSheetResource::SaveParsedStyleSheet(StyleSheetContents* sheet) {
  DCHECK(sheet);
  DCHECK(sheet->IsCacheableForResource());

  if (!MemoryCache::Get()->Contains(this)) {
    // This stylesheet resource did conflict with another resource and was not
    // added to the cache.
    SetParsedStyleSheetCache(nullptr);
    return;
  }
  SetParsedStyleSheetCache(sheet);
}

void CSSStyleSheetResource::SetDecodedSheetText(
    const String& decoded_sheet_text) {
  decoded_sheet_text_ = decoded_sheet_text;
  UpdateDecodedSize();
}

void CSSStyleSheetResource::UpdateDecodedSize() {
  size_t decoded_size = decoded_sheet_text_.CharactersSizeInBytes();
  if (parsed_style_sheet_cache_)
    decoded_size += parsed_style_sheet_cache_->EstimatedSizeInBytes();
  SetDecodedSize(decoded_size);
}

void CSSStyleSheetResource::AdvanceLoadingState(LoadingState new_state) {
  switch (loading_state_) {
    case LoadingState::kLoading:
      CHECK(new_state == LoadingState::kTokenizing ||
            new_state == LoadingState::kFinished);
      break;
    case LoadingState::kTokenizing:
      CHECK(new_state == LoadingState::kFinished);
      break;
    case LoadingState::kFinished:
      CHECK(new_state == LoadingState::kLoading && IsCacheValidator());
      break;
  }

  loading_state_ = new_state;

  // If we're done loading, either no data was received or the decoded text
  // should be set.
  if (loading_state_ == LoadingState::kFinished)
    DCHECK(!Data() || !decoded_sheet_text_.IsNull());
}

}  // namespace blink
