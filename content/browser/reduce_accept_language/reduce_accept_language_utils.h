// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_REDUCE_ACCEPT_LANGUAGE_REDUCE_ACCEPT_LANGUAGE_UTILS_H_
#define CONTENT_BROWSER_REDUCE_ACCEPT_LANGUAGE_REDUCE_ACCEPT_LANGUAGE_UTILS_H_

#include "content/common/content_export.h"
#include "content/public/browser/reduce_accept_language_controller_delegate.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/mojom/parsed_headers.mojom-forward.h"
#include "services/network/public/mojom/variants_header.mojom.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy.h"
#include "url/gurl.h"

namespace content {

class BrowserContext;
class FrameTreeNode;

// This class is a collection of utils used by navigation requests to reduce the
// fingerprinting surface of the Accept-Language header. See
// https://github.com/Tanych/accept-language.
class CONTENT_EXPORT ReduceAcceptLanguageUtils {
 public:
  explicit ReduceAcceptLanguageUtils(
      ReduceAcceptLanguageControllerDelegate& delegate);
  ~ReduceAcceptLanguageUtils();

  // No copy constructor and no copy assignment operator.
  ReduceAcceptLanguageUtils(const ReduceAcceptLanguageUtils&) = delete;
  ReduceAcceptLanguageUtils& operator=(const ReduceAcceptLanguageUtils&) =
      delete;

  // Create and return a ReduceAcceptLanguageUtils instance based on provided
  // `browser_context`.
  static absl::optional<ReduceAcceptLanguageUtils> Create(
      BrowserContext* browser_context);

  // Returns true if `accept_language` matches `content_language` using the
  // Basic Filtering scheme. See RFC4647 of Section 3.3.
  static bool DoesAcceptLanguageMatchContentLanguage(
      const std::string& accept_language,
      const std::string& content_language);

  // Starting from each preferred language in `preferred_languages` in order,
  // return the first matched language if the language matches any language in
  // `available_languages`, otherwise return absl::nullopt. The matching
  // algorithm is that if any language in `available_languages` is a wildcard or
  // matches the language `preferred_languages`, return the matched language as
  // preferred language.
  static absl::optional<std::string> GetFirstMatchPreferredLanguage(
      const std::vector<std::string>& preferred_languages,
      const std::vector<std::string>& available_languages);

  // Returns whether reduce accept language can happen for the given URL.
  // This is true only if the URL is eligible.
  //
  // `request_origin` is the origin to be used for reduced accept language
  // storage.
  //
  // TODO(crbug.com/1323776) confirm with CSP sandbox owner if language
  // preferences need to be hidden from sandboxed origins.
  static bool ShouldReduceAcceptLanguage(const url::Origin& request_origin);

  // Updates the accept-language present in headers and returns the reduced
  // accept language added to accept-language header. This is called when
  // NavigationRequest was created and when language value changes after
  // the NavigationRequest was created.
  //
  // See `ShouldReduceAcceptLanguage` for `request_origin`.
  absl::optional<std::string> AddNavigationRequestAcceptLanguageHeaders(
      const url::Origin& request_origin,
      FrameTreeNode* frame_tree_node,
      net::HttpRequestHeaders* headers);

  // Reads incoming language and persists it to HostContentSettingsMap prefs
  // storage as appropriate. Returns a bool indicating whether it needs to
  // resend the request.
  bool ReadAndPersistAcceptLanguageForNavigation(
      const url::Origin& request_origin,
      const net::HttpRequestHeaders& request_headers,
      const network::mojom::ParsedHeadersPtr& parsed_headers);

  // Looks up which reduced accept language should be used.
  //
  // This is based on the top-level document's origin.
  // - For main frame navigation, this is the origin of the new document to
  //   commit, given by `request_origin`.
  // - For iframe navigations, this is the current top-level document's origin
  //   retrieved via `frame_tree_node`.
  //
  // See `ShouldReduceAcceptLanguage` for `request_origin`.
  absl::optional<std::string> LookupReducedAcceptLanguage(
      const url::Origin& request_origin,
      FrameTreeNode* frame_tree_node);

 private:
  // Captures the state used in applying persist accept language.
  struct PersistLanguageResult {
    PersistLanguageResult();
    PersistLanguageResult(const PersistLanguageResult& other);
    ~PersistLanguageResult();

    // If true, navigation request needs to resend the requests with the
    // modified accept language header.
    bool should_resend_request = false;
    absl::optional<std::string> language_to_persist = absl::nullopt;
  };

  // Returns whether to persist a language selection based on the given language
  // information at response time, and also whether the request needs to be
  // restarted.
  PersistLanguageResult GetLanguageToPersist(
      const std::string& initial_accept_language,
      const std::vector<std::string>& content_languages,
      const std::vector<std::string>& preferred_languages,
      const std::vector<std::string>& available_languages);

  // Return the reduced accept language of the top-level document origin.
  absl::optional<std::string> GetTopLevelDocumentOriginReducedAcceptLanguage(
      const url::Origin& request_origin,
      FrameTreeNode* frame_tree_node);

  // The delegate is owned by the BrowserContext, which should always outlive
  // this utility class.
  raw_ref<ReduceAcceptLanguageControllerDelegate> delegate_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_REDUCE_ACCEPT_LANGUAGE_REDUCE_ACCEPT_LANGUAGE_UTILS_H_
