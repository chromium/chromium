// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/reduce_accept_language/reduce_accept_language_utils.h"

#include "base/strings/string_util.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "services/network/public/cpp/features.h"
#include "url/origin.h"

namespace content {

namespace {

using ::network::mojom::VariantsHeaderPtr;

const char kAcceptLanguageLowerCase[] = "accept-language";

std::string GetFirstUserAcceptLanguage(
    const std::vector<std::string>& user_accept_language) {
  DCHECK(user_accept_language.size() > 0);
  // Return the first user's accept-language. User accept language shouldn't be
  // empty since we read from language prefs. If it's empty, we need to catch up
  // this case to known any major issue.
  return user_accept_language[0];
}

}  // namespace

ReduceAcceptLanguageUtils::PersistLanguageResult::PersistLanguageResult() =
    default;
ReduceAcceptLanguageUtils::PersistLanguageResult::PersistLanguageResult(
    const PersistLanguageResult& other) = default;
ReduceAcceptLanguageUtils::PersistLanguageResult::~PersistLanguageResult() =
    default;

ReduceAcceptLanguageUtils::ReduceAcceptLanguageUtils(
    ReduceAcceptLanguageControllerDelegate& delegate)
    : delegate_(delegate) {}

ReduceAcceptLanguageUtils::~ReduceAcceptLanguageUtils() = default;

// static
absl::optional<ReduceAcceptLanguageUtils> ReduceAcceptLanguageUtils::Create(
    BrowserContext* browser_context) {
  DCHECK(browser_context);
  if (!base::FeatureList::IsEnabled(network::features::kReduceAcceptLanguage))
    return absl::nullopt;
  ReduceAcceptLanguageControllerDelegate* reduce_accept_lang_delegate =
      browser_context->GetReduceAcceptLanguageControllerDelegate();
  if (!reduce_accept_lang_delegate)
    return absl::nullopt;
  return absl::make_optional<ReduceAcceptLanguageUtils>(
      *reduce_accept_lang_delegate);
}

// static
bool ReduceAcceptLanguageUtils::DoesAcceptLanguageMatchContentLanguage(
    const std::string& accept_language,
    const std::string& content_language) {
  return content_language == "*" ||
         base::EqualsCaseInsensitiveASCII(accept_language, content_language) ||
         // Check whether `accept-language` has the same base language with
         // `content-language`, e.g. Accept-Language: en-US will be considered a
         // match for Content-Language: en.
         (base::StartsWith(accept_language, content_language,
                           base::CompareCase::INSENSITIVE_ASCII) &&
          accept_language[content_language.size()] == '-');
}

// static
bool ReduceAcceptLanguageUtils::ShouldReduceAcceptLanguage(
    const url::Origin& request_origin) {
  return request_origin.GetURL().SchemeIsHTTPOrHTTPS();
}

absl::optional<std::string>
ReduceAcceptLanguageUtils::GetFirstMatchPreferredLanguage(
    const std::vector<std::string>& preferred_languages,
    const std::vector<std::string>& available_languages) {
  // Match the language in priority order.
  for (const auto& preferred_language : preferred_languages) {
    for (const auto& available_language : available_languages) {
      if (available_language == "*" ||
          base::EqualsCaseInsensitiveASCII(preferred_language,
                                           available_language)) {
        return preferred_language;
      }
    }
  }
  // If the site's available languages don't match any of the user's preferred
  // languages, then browser won't do anything further.
  return absl::nullopt;
}

absl::optional<std::string>
ReduceAcceptLanguageUtils::AddNavigationRequestAcceptLanguageHeaders(
    const url::Origin& request_origin,
    FrameTreeNode* frame_tree_node,
    net::HttpRequestHeaders* headers) {
  DCHECK(headers);

  absl::optional<std::string> reduced_accept_language =
      LookupReducedAcceptLanguage(request_origin, frame_tree_node);
  if (reduced_accept_language) {
    headers->SetHeader(net::HttpRequestHeaders::kAcceptLanguage,
                       reduced_accept_language.value());
  }
  return reduced_accept_language;
}

bool ReduceAcceptLanguageUtils::ReadAndPersistAcceptLanguageForNavigation(
    const url::Origin& request_origin,
    const net::HttpRequestHeaders& request_headers,
    const network::mojom::ParsedHeadersPtr& parsed_headers) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(parsed_headers);

  if (!parsed_headers->content_language || !parsed_headers->variants_headers)
    return false;

  if (!ShouldReduceAcceptLanguage(request_origin))
    return false;

  // Only parse and persist if the Variants headers include Accept-Language.
  auto variants_accept_lang_iter = base::ranges::find_if(
      parsed_headers->variants_headers.value(),
      [](const VariantsHeaderPtr& variants_header) {
        return variants_header->name == kAcceptLanguageLowerCase;
      });
  if (variants_accept_lang_iter ==
      parsed_headers->variants_headers.value().end()) {
    return false;
  }

  std::string initial_accept_language;
  if (!request_headers.GetHeader(net::HttpRequestHeaders::kAcceptLanguage,
                                 &initial_accept_language)) {
    return false;
  }

  PersistLanguageResult persist_params = GetLanguageToPersist(
      initial_accept_language, parsed_headers->content_language.value(),
      delegate_->GetUserAcceptLanguages(),
      (*variants_accept_lang_iter)->available_values);

  if (persist_params.language_to_persist) {
    delegate_->PersistReducedLanguage(
        request_origin, persist_params.language_to_persist.value());
  }

  return persist_params.should_resend_request;
}

absl::optional<std::string>
ReduceAcceptLanguageUtils::LookupReducedAcceptLanguage(
    const url::Origin& request_origin,
    FrameTreeNode* frame_tree_node) {
  DCHECK(frame_tree_node);

  if (!base::FeatureList::IsEnabled(network::features::kReduceAcceptLanguage) ||
      !ShouldReduceAcceptLanguage(request_origin)) {
    return absl::nullopt;
  }

  const absl::optional<std::string>& preferred_language =
      GetTopLevelDocumentOriginReducedAcceptLanguage(request_origin,
                                                     frame_tree_node);

  const std::vector<std::string>& user_accept_languages =
      delegate_->GetUserAcceptLanguages();
  if (!preferred_language) {
    return GetFirstUserAcceptLanguage(user_accept_languages);
  }

  // If the preferred language stored by the delegate doesn't match any of the
  // user's currently preferred Accept-Languages, then the user might have
  // changed their preferences since the result was stored. In this case, use
  // the first Accept-Language instead.
  //
  // TODO(crbug.com/1323776) make sure the delegate clears its cache if the
  // user's preferences changed.
  auto iter = base::ranges::find_if(
      user_accept_languages, [&](const std::string& language) {
        return DoesAcceptLanguageMatchContentLanguage(
            language, preferred_language.value());
      });
  return iter != user_accept_languages.end()
             ? preferred_language
             : GetFirstUserAcceptLanguage(user_accept_languages);
}

absl::optional<std::string>
ReduceAcceptLanguageUtils::GetTopLevelDocumentOriginReducedAcceptLanguage(
    const url::Origin& request_origin,
    FrameTreeNode* frame_tree_node) {
  // The reduced accept language should be based on the outermost main
  // document's origin in most cases. An empty or opaque origin will result in a
  // nullopt return value. If this call is being made for the outermost main
  // document, then the NavigationRequest has not yet committed and we must use
  // the origin from the in-flight NavigationRequest. Otherwise, subframes and
  // sub-pages (except Fenced Frames) can use the outermost main document's last
  // committed origin.
  //
  // TODO(https://github.com/WICG/fenced-frame/issues/39) decide whether
  // Fenced Frames should be treated as an internally-consistent Page, with
  // language negotiation for the inner main document and/or subframes
  // that match the main document.
  url::Origin outermost_main_rfh_origin;
  if (frame_tree_node->IsOutermostMainFrame()) {
    outermost_main_rfh_origin = request_origin;
  } else if (!frame_tree_node->IsInFencedFrameTree()) {
    RenderFrameHostImpl* outermost_main_rfh =
        frame_tree_node->frame_tree()->GetMainFrame()->GetOutermostMainFrame();
    outermost_main_rfh_origin = outermost_main_rfh->GetLastCommittedOrigin();
  }

  // Record the time spent getting the reduced accept language to better
  // understand whether this prefs read can introduce any large latency.
  const base::TimeTicks start_time = base::TimeTicks::Now();
  const absl::optional<std::string>& preferred_language =
      delegate_->GetReducedLanguage(outermost_main_rfh_origin);
  base::TimeDelta duration = base::TimeTicks::Now() - start_time;
  base::UmaHistogramTimes("ReduceAcceptLanguage.FetchLatency", duration);
  return preferred_language;
}

ReduceAcceptLanguageUtils::PersistLanguageResult
ReduceAcceptLanguageUtils::GetLanguageToPersist(
    const std::string& initial_accept_language,
    const std::vector<std::string>& content_languages,
    const std::vector<std::string>& preferred_languages,
    const std::vector<std::string>& available_languages) {
  DCHECK(preferred_languages.size() > 0);

  PersistLanguageResult result;

  // If the response content-language matches the initial accept language
  // values, no need to resend the request.
  auto iter = base::ranges::find_if(content_languages, [&](const std::string&
                                                               language) {
    return ReduceAcceptLanguageUtils::DoesAcceptLanguageMatchContentLanguage(
        initial_accept_language, language);
  });
  std::string selected_language;
  if (iter != content_languages.end()) {
    selected_language = initial_accept_language;
  } else {
    // If content-language doesn't match initial accept-language and the site
    // has available languages matching one of the the user's preferences, then
    // the browser should resend the request with the top matching language.
    const absl::optional<std::string>& matched_language =
        ReduceAcceptLanguageUtils::GetFirstMatchPreferredLanguage(
            preferred_languages, available_languages);
    if (matched_language) {
      selected_language = matched_language.value();

      // Only resend request if the `matched_language` doesn't match any
      // content languages in current response header because otherwise
      // resending the request won't get a better result.
      auto matched_iter = base::ranges::find_if(
          content_languages, [&](const std::string& language) {
            return base::EqualsCaseInsensitiveASCII(language,
                                                    matched_language.value());
          });
      result.should_resend_request = (matched_iter == content_languages.end());
    }
  }

  // Only persist the language of choice for an origin if it differs from
  // the user’s first preferred language because we can directly access the
  // user’s first preferred language from language prefs.
  if (!selected_language.empty() &&
      selected_language != preferred_languages[0]) {
    result.language_to_persist = selected_language;
  }
  return result;
}

}  // namespace content
