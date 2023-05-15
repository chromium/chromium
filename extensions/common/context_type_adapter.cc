// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/context_type_adapter.h"

#include "base/notreached.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/mojom/frame.mojom.h"

namespace extensions {

mojom::ContextType FeatureContextToMojomContext(
    Feature::Context feature_context) {
  switch (feature_context) {
    case Feature::BLESSED_EXTENSION_CONTEXT:
      return mojom::ContextType::kPrivilegedExtension;
    case Feature::UNBLESSED_EXTENSION_CONTEXT:
      return mojom::ContextType::kUnprivilegedExtension;
    case Feature::CONTENT_SCRIPT_CONTEXT:
      return mojom::ContextType::kContentScript;
    case Feature::WEB_PAGE_CONTEXT:
      return mojom::ContextType::kWebPage;
    case Feature::BLESSED_WEB_PAGE_CONTEXT:
      return mojom::ContextType::kPrivilegedWebPage;
    case Feature::WEBUI_CONTEXT:
      return mojom::ContextType::kWebUi;
    case Feature::WEBUI_UNTRUSTED_CONTEXT:
      return mojom::ContextType::kUntrustedWebUi;
    case Feature::LOCK_SCREEN_EXTENSION_CONTEXT:
      return mojom::ContextType::kLockscreenExtension;
    case Feature::OFFSCREEN_EXTENSION_CONTEXT:
      return mojom::ContextType::kOffscreenExtension;
    case Feature::USER_SCRIPT_CONTEXT:
      return mojom::ContextType::kUserScript;
    case Feature::UNSPECIFIED_CONTEXT:
      // We never expect to receive an unspecified context. We separate this
      // NOTREACHED_NORETURN() (and don't just `break;`) to get separate reports
      // if we do ever hit this.
      NOTREACHED_NORETURN();
  }

  NOTREACHED_NORETURN();
}

Feature::Context MojomContextToFeatureContext(
    mojom::ContextType mojom_context) {
  switch (mojom_context) {
    case mojom::ContextType::kPrivilegedExtension:
      return Feature::BLESSED_EXTENSION_CONTEXT;
    case mojom::ContextType::kUnprivilegedExtension:
      return Feature::UNBLESSED_EXTENSION_CONTEXT;
    case mojom::ContextType::kContentScript:
      return Feature::CONTENT_SCRIPT_CONTEXT;
    case mojom::ContextType::kWebPage:
      return Feature::WEB_PAGE_CONTEXT;
    case mojom::ContextType::kPrivilegedWebPage:
      return Feature::BLESSED_WEB_PAGE_CONTEXT;
    case mojom::ContextType::kWebUi:
      return Feature::WEBUI_CONTEXT;
    case mojom::ContextType::kUntrustedWebUi:
      return Feature::WEBUI_UNTRUSTED_CONTEXT;
    case mojom::ContextType::kLockscreenExtension:
      return Feature::LOCK_SCREEN_EXTENSION_CONTEXT;
    case mojom::ContextType::kOffscreenExtension:
      return Feature::OFFSCREEN_EXTENSION_CONTEXT;
    case mojom::ContextType::kUserScript:
      return Feature::USER_SCRIPT_CONTEXT;
  }
}

}  // namespace extensions
