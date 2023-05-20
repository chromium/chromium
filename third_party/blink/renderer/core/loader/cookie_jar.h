// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_COOKIE_JAR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_COOKIE_JAR_H_

#include "services/network/public/mojom/restricted_cookie_manager.mojom-blink.h"

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {
class Document;

class CookieJar : public GarbageCollected<CookieJar> {
 public:
  explicit CookieJar(blink::Document* document);
  virtual ~CookieJar();
  void Trace(Visitor* visitor) const;

  void SetCookie(const String& value);
  String Cookies();
  bool CookiesEnabled();
  void SetCookieManager(
      mojo::PendingRemote<network::mojom::blink::RestrictedCookieManager>
          cookie_manager);

  // Invalidate cached string. To be called explicitly from Document. This is
  // used in cases where a Document action could change the ability for
  // CookieJar to return values to JS without changing the value of the cookies
  // themselves. For example changing storage access can stop the JS from being
  // able to access the document's Cookie without the value ever changing. In
  // that case it's faulty to treat a subsequent request as a cache hit so we
  // invalidate.
  void InvalidateCache();

 private:
  bool RequestRestrictedCookieManagerIfNeeded();

  // Updates the fake cookie cache after a
  // RestrictedCookieManager::GetCookiesString request returns.
  //
  // We want to evaluate the possible performance gain from having a cookie
  // cache. There is no real cache right now and this class just stores a hash
  // to determine if the current request could have been served from a real
  // cache.
  void UpdateCacheAfterGetRequest(const KURL& cookie_url,
                                  const String& cookie_string);

  HeapMojoRemote<network::mojom::blink::RestrictedCookieManager> backend_;
  Member<blink::Document> document_;

  // Hash used to determine if the value returned by a call to
  // RestrictedCookieManager::GetCookiesString is the same as a previous one.
  // Used to answer the question: "had we keep the last cookie_string around
  // would it have been possible to return that instead of making a new IPC?".
  // Combines hashes for the `cookie_string` returned by the call and the
  // `cookie_url` used as a parameter to the call.
  //
  // ATTENTION: Just use hashes for now to keep space overhead low, but more
  // importantly, because keeping cookies around is tricky from a security
  // perspective.
  absl::optional<unsigned> last_cookies_hash_;
  // Whether the last operation performed on this jar was a set or get. Used
  // along with `last_cookies_hash_` when updating the histogram that tracks
  // cookie access results.
  bool last_operation_was_set_{false};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_COOKIE_JAR_H_
