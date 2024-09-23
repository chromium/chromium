// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_EXPOSED_ISOLATION_INFO_H_
#define CONTENT_BROWSER_WEB_EXPOSED_ISOLATION_INFO_H_

#include <optional>

#include "content/common/content_export.h"
#include "url/origin.h"

namespace content {

// Represents the isolation level of a page or group of pages. This is used
// for process allocation and to selectively enable powerful features such
// as SharedArrayBuffer and Direct Sockets.
//
// Currently, three levels of isolation are represented:
//
// 1.  Non-isolated contexts.
//
// 2.  Cross-origin isolation, as defined in
//     https://html.spec.whatwg.org/multipage/document-sequences.html#cross-origin-isolation-mode.
//     This is computed purely by examining Cross-Origin-Opener-Policy and
//     Cross-Origin-Embedder-Policy headers on a given response.
//
// 3.  Isolated Application contexts, which correspond to Isolated Contexts as
//     defined in:
//     https://wicg.github.io/isolated-web-apps/isolated-contexts.html
//     These contexts have higher isolation and integrity requirements than
//     cross-origin isolation. The embedder is responsible for deciding whether
//     a particular cross-origin isolated environment can qualify for this
//     isolation level.
class CONTENT_EXPORT WebExposedIsolationInfo {
 public:
  static WebExposedIsolationInfo CreateNonIsolated();
  static WebExposedIsolationInfo CreateIsolated(const url::Origin& origin);
  static WebExposedIsolationInfo CreateIsolatedApplication(
      const url::Origin& origin);

  // These helpers make it easy to compare against an optional
  // WebExposedIsolationInfo. This is useful because a navigation may return
  // an empty WebExposedIsolationInfo to the process model, for example when
  // we do not yet have a final network response. In that case it is considered
  // compatible with any WebExposedIsolationInfo.
  //
  // In details, the underlying logic is as follow:
  // - Two valid values are compared using the == operator.
  // - Null and a valid value returns true.
  // - Two null values returns true.
  static bool AreCompatible(const WebExposedIsolationInfo& a,
                            const WebExposedIsolationInfo& b);
  static bool AreCompatible(const WebExposedIsolationInfo& a,
                            const std::optional<WebExposedIsolationInfo>& b);
  static bool AreCompatible(const std::optional<WebExposedIsolationInfo>& a,
                            const WebExposedIsolationInfo& b);
  static bool AreCompatible(const std::optional<WebExposedIsolationInfo>& a,
                            const std::optional<WebExposedIsolationInfo>& b);

  WebExposedIsolationInfo(const WebExposedIsolationInfo& other);
  ~WebExposedIsolationInfo();

  // Returns `true` for isolated contexts created via `CreateIsolated()` or
  // `CreateIsolatedApplication()`, and false for contexts created via
  // `CreateNonIsolated()`.
  //
  // This corresponds to "cross-origin isolation" as defined in HTML:
  // https://html.spec.whatwg.org/multipage/document-sequences.html#cross-origin-isolation-mode
  bool is_isolated() const { return origin_.has_value(); }

  // Returns `true` for contexts created via `CreateIsolatedApplication()`, and
  // `false` for those created via `CreateNonIsolated()` or `CreatedIsolated()`.
  //
  // This corresponds to "isolated contexts" as defined here:
  // https://wicg.github.io/isolated-web-apps/isolated-contexts.html
  bool is_isolated_application() const {
    return origin_.has_value() && isolated_application_;
  }

  // Returns the top level origin shared across pages with this cross-origin
  // isolation status. This only returns a value if is_isolated is true.
  const url::Origin& origin() const;

  bool operator==(const WebExposedIsolationInfo& b) const;
  bool operator!=(const WebExposedIsolationInfo& b) const;

  // Non-isolated < Isolated < Isolated Application.
  //
  // All non-isolated contexts are equivalent.
  //
  // Origin comparisons determine ordering of isolated contexts.
  bool operator<(const WebExposedIsolationInfo& b) const;

 private:
  WebExposedIsolationInfo(const std::optional<url::Origin>& origin,
                          bool isolated_application);

  // |origin_| serve two purposes. If null, it indicates that the page(s) it
  // refers to are not isolated, and that the crossOriginIsolated boolean is
  // false. If it has a value, all these page(s) share the same top level
  // origin. This ensure we can put them in the same process.
  std::optional<url::Origin> origin_;

  // Some applications may require additional isolation above and beyond what
  // COOP/COEP-based COI provides. This boolean will be `true` for applications
  // that have opted into such a context.
  //
  // TODO(mkwst): Improve the description of the Isolated Application context as
  // we work out what it is: https://crbug.com/1206150.
  bool isolated_application_ = false;
};

CONTENT_EXPORT std::ostream& operator<<(std::ostream& out,
                                        const WebExposedIsolationInfo& info);

// Disable these operators, AreCompatible() functions should be used instead.
CONTENT_EXPORT bool operator==(const std::optional<WebExposedIsolationInfo>& a,
                               const std::optional<WebExposedIsolationInfo>& b);
CONTENT_EXPORT bool operator==(const WebExposedIsolationInfo& a,
                               const std::optional<WebExposedIsolationInfo>& b);
CONTENT_EXPORT bool operator==(const std::optional<WebExposedIsolationInfo>& a,
                               const WebExposedIsolationInfo& b);
CONTENT_EXPORT bool operator!=(const std::optional<WebExposedIsolationInfo>& a,
                               const std::optional<WebExposedIsolationInfo>& b);
CONTENT_EXPORT bool operator!=(const WebExposedIsolationInfo& a,
                               const std::optional<WebExposedIsolationInfo>& b);
CONTENT_EXPORT bool operator!=(const std::optional<WebExposedIsolationInfo>& a,
                               const WebExposedIsolationInfo& b);

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_EXPOSED_ISOLATION_INFO_H_
