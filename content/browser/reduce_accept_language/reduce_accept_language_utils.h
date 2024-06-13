// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_REDUCE_ACCEPT_LANGUAGE_REDUCE_ACCEPT_LANGUAGE_UTILS_H_
#define CONTENT_BROWSER_REDUCE_ACCEPT_LANGUAGE_REDUCE_ACCEPT_LANGUAGE_UTILS_H_

#include "content/common/content_export.h"
#include "content/public/browser/origin_trials_controller_delegate.h"
#include "content/public/browser/reduce_accept_language_controller_delegate.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/mojom/parsed_headers.mojom-forward.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
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
  static std::optional<ReduceAcceptLanguageUtils> Create(
      BrowserContext* browser_context);

  // Returns true if `accept_language` matches `content_language` using the
  // Basic Filtering scheme. See RFC4647 of Section 3.3.
  static bool DoesAcceptLanguageMatchContentLanguage(
      const std::string& accept_language,
      const std::string& content_language);

  // Starting from each preferred language in `preferred_languages` in order,
  // return the first matched language if the language matches any language in
  // `available_languages`, otherwise return std::nullopt. The matching
  // algorithm is that if any language in `available_languages` is a wildcard or
  // matches the language `preferred_languages`, return the matched language as
  // preferred language.
  static std::optional<std::string> GetFirstMatchPreferredLanguage(
      const std::vector<std::string>& preferred_languages,
      const std::vector<std::string>& available_languages);

  // Returns whether reduce accept language can happen for the given URL.
  // This is true only if the URL is eligible.
  //
  // `request_origin` is the origin to be used for reduced accept language
  // storage.
  //
  // TODO(crbug.com/40224802) confirm with CSP sandbox owner if language
  // preferences need to be hidden from sandboxed origins.
  static bool OriginCanReduceAcceptLanguage(const url::Origin& request_origin);

  // Return true if the given `request_origin` opted into the
  // ReduceAcceptLanguage deprecation origin trial. This method can only be
  // called on the UI thread.
  static bool CheckDisableReduceAcceptLanguageOriginTrial(
      const GURL& request_url,
      FrameTreeNode* frame_tree_node,
      OriginTrialsControllerDelegate* origin_trials_delegate);

  // Updates the accept-language present in headers and returns the reduced
  // accept language added to accept-language header. This is called when
  // NavigationRequest was created and when language value changes after
  // the NavigationRequest was created.
  //
  // See `OriginCanReduceAcceptLanguage` for `request_origin`.
  std::optional<std::string> AddNavigationRequestAcceptLanguageHeaders(
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
  // Warning: This could potentially clear the persisted language in pref
  // storage if the persisted language can't be found in the user's
  // Accept-Language.
  //
  // This is based on the top-level document's origin.
  // - For main frame navigation, this is the origin of the new document to
  //   commit, given by `request_origin`.
  // - For iframe navigations, this is the current top-level document's origin
  //   retrieved via `frame_tree_node`.
  //
  // See `OriginCanReduceAcceptLanguage` for `request_origin`.
  std::optional<std::string> LookupReducedAcceptLanguage(
      const url::Origin& request_origin,
      FrameTreeNode* frame_tree_node);

  // Remove the persisted language for the given top-level document's `origin`.
  void RemoveReducedAcceptLanguage(const url::Origin& origin,
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
    std::optional<std::string> language_to_persist = std::nullopt;
  };

  // Returns whether to persist a language selection based on the given language
  // information at response time, and also whether the request needs to be
  // restarted.
  PersistLanguageResult GetLanguageToPersist(
      const std::string& initial_accept_language,
      const std::vector<std::string>& content_languages,
      const std::vector<std::string>& preferred_languages,
      const std::vector<std::string>& available_languages);

  // Return the origin to look up the persisted language.
  //
  // The reduced accept language should be based on the outermost main
  // document's origin in most cases. If this call is being made for the
  // outermost main document, then the NavigationRequest has not yet committed
  // and we must use the origin from the in-flight NavigationRequest. Subframes
  // and sub-pages (except Fenced Frames) can use the outermost main document's
  // last committed origin. Otherwise, it will result in a nullopt return value.
  //
  // TODO(https://github.com/WICG/fenced-frame/issues/39) decide whether
  // Fenced Frames should be treated as an internally-consistent Page, with
  // language negotiation for the inner main document and/or subframes
  // that match the main document.
  static std::optional<url::Origin> GetOriginForLanguageLookup(
      const url::Origin& request_origin,
      FrameTreeNode* frame_tree_node);

  // The delegate is owned by the BrowserContext, which should always outlive
  // this utility class.
  raw_ref<ReduceAcceptLanguageControllerDelegate, DanglingUntriaged> delegate_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_REDUCE_ACCEPT_LANGUAGE_REDUCE_ACCEPT_LANGUAGE_UTILS_H_
