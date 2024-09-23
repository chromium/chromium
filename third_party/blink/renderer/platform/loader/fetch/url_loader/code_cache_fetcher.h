// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_URL_LOADER_CODE_CACHE_FETCHER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_URL_LOADER_CODE_CACHE_FETCHER_H_

#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "third_party/blink/public/mojom/loader/code_cache.mojom-blink-forward.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"

namespace network {
struct ResourceRequest;
}  // namespace network

namespace blink {

class CodeCacheHost;

// Handles the fetching and validation of code cache entries.
class BLINK_PLATFORM_EXPORT CodeCacheFetcher
    : public WTF::RefCounted<CodeCacheFetcher> {
 public:
  static scoped_refptr<CodeCacheFetcher> TryCreateAndStart(
      const network::ResourceRequest& request,
      CodeCacheHost& code_cache_host,
      base::OnceClosure done_closure);

  CodeCacheFetcher(CodeCacheHost& code_cache_host,
                   mojom::blink::CodeCacheType code_cache_type,
                   const KURL& url,
                   base::OnceClosure done_closure);

  CodeCacheFetcher(const CodeCacheFetcher&) = delete;
  CodeCacheFetcher& operator=(const CodeCacheFetcher&) = delete;

  bool is_waiting() const { return is_waiting_; }

  void SetCurrentUrl(const KURL& new_url) { current_url_ = new_url; }
  void DidReceiveCachedMetadataFromUrlLoader();
  std::optional<mojo_base::BigBuffer> TakeCodeCacheForResponse(
      const network::mojom::URLResponseHead& response_head);

 private:
  friend class WTF::RefCounted<CodeCacheFetcher>;
  ~CodeCacheFetcher() = default;

  void Start();

  void DidReceiveCachedCode(base::Time response_time,
                            mojo_base::BigBuffer data);

  void ClearCodeCacheEntryIfPresent();

  base::WeakPtr<CodeCacheHost> code_cache_host_;
  mojom::blink::CodeCacheType code_cache_type_;

  // The initial URL used for code cache fetching, prior to redirects. This
  // should match the initial URL for script fetching.
  const KURL initial_url_;

  // The current URL used for code cache fetching. This should match / will be
  // updated to the current url for script fetching (either initial or
  // redirected).
  KURL current_url_;

  base::OnceClosure done_closure_;

  bool is_waiting_ = true;
  bool did_receive_cached_metadata_from_url_loader_ = false;
  std::optional<mojo_base::BigBuffer> code_cache_data_;
  base::Time code_cache_response_time_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_URL_LOADER_CODE_CACHE_FETCHER_H_
