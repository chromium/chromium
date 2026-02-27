// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_NAVIGATION_API_NAVIGATION_TYPE_UTIL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_NAVIGATION_API_NAVIGATION_TYPE_UTIL_H_

#include "base/notreached.h"
#include "third_party/blink/public/mojom/navigation/navigation_type_for_navigation_api.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/navigation/navigation_type_for_navigation_api.mojom-shared.h"
#include "third_party/blink/public/web/web_frame_load_type.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_navigation_type.h"

namespace blink {

// Returns the web-exposed navigation type for the given `WebFrameLoadType`,
// converting it to the appropriate value.
constexpr V8NavigationType::Enum ToV8NavigationType(WebFrameLoadType type) {
  switch (type) {
    case WebFrameLoadType::kStandard:
      return V8NavigationType::Enum::kPush;
    case WebFrameLoadType::kBackForward:
    case WebFrameLoadType::kRestore:
      return V8NavigationType::Enum::kTraverse;
    case WebFrameLoadType::kReload:
    case WebFrameLoadType::kReloadBypassingCache:
      return V8NavigationType::Enum::kReload;
    case WebFrameLoadType::kReplaceCurrentItem:
      return V8NavigationType::Enum::kReplace;
  }
  NOTREACHED();
}

// Returns the `WebFrameLoadType` for the given web-exposed navigation type.
//
// Note: ToWebFrameLoadType(ToV8NavigationType(load_type)) will not necessarily
// return load_type since there is not a 1:1 conversion from `WebFrameLoadType`.
constexpr WebFrameLoadType ToWebFrameLoadType(V8NavigationType::Enum type) {
  switch (type) {
    case V8NavigationType::Enum::kPush:
      return WebFrameLoadType::kStandard;
    case V8NavigationType::Enum::kReplace:
      return WebFrameLoadType::kReplaceCurrentItem;
    case V8NavigationType::Enum::kTraverse:
      return WebFrameLoadType::kBackForward;
    case V8NavigationType::Enum::kReload:
      return WebFrameLoadType::kReload;
  }
  NOTREACHED();
}

// Returns the mojom enum value for the given web-exposed enum value. The
// conversion is 1:1.
constexpr mojom::blink::NavigationTypeForNavigationApi
ToNavigationTypeForNavigationApi(V8NavigationType::Enum type) {
  switch (type) {
    case V8NavigationType::Enum::kPush:
      return mojom::blink::NavigationTypeForNavigationApi::kPush;
    case V8NavigationType::Enum::kTraverse:
      return mojom::blink::NavigationTypeForNavigationApi::kTraverse;
    case V8NavigationType::Enum::kReload:
      return mojom::blink::NavigationTypeForNavigationApi::kReload;
    case V8NavigationType::Enum::kReplace:
      return mojom::blink::NavigationTypeForNavigationApi::kReplace;
  }
  NOTREACHED();
}

// Returns the web-exposed navigation type for the given mojom enum value. The
// conversion is 1:1.
constexpr V8NavigationType::Enum ToV8NavigationType(
    mojom::blink::NavigationTypeForNavigationApi type) {
  switch (type) {
    case mojom::blink::NavigationTypeForNavigationApi::kPush:
      return V8NavigationType::Enum::kPush;
    case mojom::blink::NavigationTypeForNavigationApi::kTraverse:
      return V8NavigationType::Enum::kTraverse;
    case mojom::blink::NavigationTypeForNavigationApi::kReplace:
      return V8NavigationType::Enum::kReplace;
    case mojom::blink::NavigationTypeForNavigationApi::kReload:
      return V8NavigationType::Enum::kReload;
  }
  NOTREACHED();
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_NAVIGATION_API_NAVIGATION_TYPE_UTIL_H_
