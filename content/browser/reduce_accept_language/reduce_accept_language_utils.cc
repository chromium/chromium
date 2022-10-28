// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/reduce_accept_language/reduce_accept_language_utils.h"

#include "base/strings/string_util.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/public/browser/browser_context.h"
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
  auto variants_accept_lang_iter = base::ranges::find(
      parsed_headers->variants_headers.value(), kAcceptLanguageLowerCase,
      &::network::mojom::VariantsHeader::name);
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

  const absl::optional<url::Origin>& origin_for_lookup =
      GetOriginForLanguageLookup(request_origin, frame_tree_node);

  const absl::optional<std::string>& persisted_language =
      origin_for_lookup
          ? delegate_->GetReducedLanguage(origin_for_lookup.value())
          : absl::nullopt;

  const std::vector<std::string>& user_accept_languages =
      delegate_->GetUserAcceptLanguages();
  if (!persisted_language) {
    return GetFirstUserAcceptLanguage(user_accept_languages);
  }

  // Use the preferred language stored by the delegate if it matches any of the
  // user's current preferences.
  auto iter = base::ranges::find_if(
      user_accept_languages, [&](const std::string& language) {
        return DoesAcceptLanguageMatchContentLanguage(
            language, persisted_language.value());
      });
  if (iter != user_accept_languages.end()) {
    return persisted_language;
  }

  // If the preferred language stored by the delegate doesn't match any of the
  // user's currently preferred Accept-Languages, then the user might have
  // changed their preferences since the result was stored. In this case, clear
  // the persisted value and use the first Accept-Language instead.
  delegate_->ClearReducedLanguage(origin_for_lookup.value());
  return GetFirstUserAcceptLanguage(user_accept_languages);
}

absl::optional<url::Origin>
ReduceAcceptLanguageUtils::GetOriginForLanguageLookup(
    const url::Origin& request_origin,
    FrameTreeNode* frame_tree_node) {
  // See explanation in header file.
  if (frame_tree_node->IsOutermostMainFrame()) {
    return request_origin;
  } else if (!frame_tree_node->IsInFencedFrameTree()) {
    RenderFrameHostImpl* outermost_main_rfh =
        frame_tree_node->frame_tree()->GetMainFrame()->GetOutermostMainFrame();
    return outermost_main_rfh->GetLastCommittedOrigin();
  }
  return absl::nullopt;
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
  std::string selected_language;
  if (base::ranges::any_of(content_languages, [&](const std::string& language) {
        return ReduceAcceptLanguageUtils::
            DoesAcceptLanguageMatchContentLanguage(initial_accept_language,
                                                   language);
      })) {
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
      result.should_resend_request = base::ranges::none_of(
          content_languages, [&](const std::string& language) {
            return base::EqualsCaseInsensitiveASCII(language,
                                                    matched_language.value());
          });
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
