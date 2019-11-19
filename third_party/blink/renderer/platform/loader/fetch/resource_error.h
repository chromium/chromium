/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2013 Google Inc.  All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_RESOURCE_ERROR_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_RESOURCE_ERROR_H_

#include <iosfwd>
#include "base/optional.h"
#include "services/network/public/cpp/cors/cors_error_status.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

struct WebURLError;
enum class ResourceRequestBlockedReason;

// ResourceError represents an error for loading a resource. There is no
// "no-error" instance. Use Optional for nullable errors.
class PLATFORM_EXPORT ResourceError final {
  DISALLOW_NEW();

 public:
  static ResourceError CancelledError(const KURL&);
  static ResourceError CancelledDueToAccessCheckError(
      const KURL&,
      ResourceRequestBlockedReason);
  static ResourceError CancelledDueToAccessCheckError(
      const KURL&,
      ResourceRequestBlockedReason,
      const String& localized_description);

  static ResourceError CacheMissError(const KURL&);
  static ResourceError TimeoutError(const KURL&);
  static ResourceError Failure(const KURL&);

  ResourceError() = delete;
  // |error_code| must not be 0.
  ResourceError(int error_code,
                const KURL& failing_url,
                base::Optional<network::CorsErrorStatus>);
  ResourceError(const KURL& failing_url,
                const network::CorsErrorStatus& status);
  ResourceError(const WebURLError&);

  // Makes a deep copy. Useful for when you need to use a ResourceError on
  // another thread.
  ResourceError Copy() const;

  int ErrorCode() const { return error_code_; }
  const String& FailingURL() const { return failing_url_; }
  const String& LocalizedDescription() const { return localized_description_; }

  bool IsCancellation() const;
  bool IsAccessCheck() const { return is_access_check_; }
  bool HasCopyInCache() const { return has_copy_in_cache_; }
  bool IsTimeout() const;
  bool IsCacheMiss() const;
  bool WasBlockedByResponse() const;
  bool ShouldCollapseInitiator() const;
  base::Optional<ResourceRequestBlockedReason> GetResourceRequestBlockedReason()
      const;

  base::Optional<network::CorsErrorStatus> CorsErrorStatus() const {
    return cors_error_status_;
  }

  operator WebURLError() const;

  static bool Compare(const ResourceError&, const ResourceError&);

 private:
  void InitializeDescription();

  int error_code_;
  int extended_error_code_ = 0;
  KURL failing_url_;
  String localized_description_;
  bool is_access_check_ = false;
  bool has_copy_in_cache_ = false;
  bool blocked_by_subresource_filter_ = false;
  base::Optional<network::CorsErrorStatus> cors_error_status_;
};

inline bool operator==(const ResourceError& a, const ResourceError& b) {
  return ResourceError::Compare(a, b);
}
inline bool operator!=(const ResourceError& a, const ResourceError& b) {
  return !(a == b);
}

PLATFORM_EXPORT std::ostream& operator<<(std::ostream&, const ResourceError&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_RESOURCE_ERROR_H_
