// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_WEBSTORE_PRIVATE_WEBSTORE_PRIVATE_API_DELEGATE_H_
#define EXTENSIONS_BROWSER_API_WEBSTORE_PRIVATE_WEBSTORE_PRIVATE_API_DELEGATE_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "build/build_config.h"
#include "components/keyed_service/core/keyed_service_base_factory.h"
#include "components/safe_browsing/buildflags.h"
#include "extensions/buildflags/buildflags.h"

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace signin {
class IdentityManager;
}  // namespace signin

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
namespace safe_browsing {
class SafeBrowsingNavigationObserverManager;
}  // namespace safe_browsing
#endif

namespace enterprise_promotion {
class PromotionEligibilityChecker;
}  // namespace enterprise_promotion

namespace extensions {

class Extension;
class ExtensionAllowlist;

// Delegate class for WebstorePrivate API that provides embedder-specific
// functionality. This allows the WebstorePrivate API implementation in
// //extensions to call into //chrome without creating a direct dependency.
class WebstorePrivateAPIDelegate {
 public:
  virtual ~WebstorePrivateAPIDelegate() = default;

  // Gets keyed service factories required by the Web Store private API.
  virtual std::vector<KeyedServiceBaseFactory*>
  GetWebStoreAPIFactoryDependencies() = 0;

  // Returns ExtensionAllowlist associated with `context`.
  virtual ExtensionAllowlist* GetExtensionAllowlist(
      content::BrowserContext* context) = 0;

  // Returns IdentityManager associated with `context`.
  virtual signin::IdentityManager* GetIdentityManager(
      content::BrowserContext* context) = 0;

  // Shows a dialog to notify the user that the extension installation is
  // blocked due to policy.
  virtual void ShowExtensionInstallBlockedDialog(
      content::WebContents* web_contents,
      const Extension* extension,
      const std::u16string& custom_error_message,
      const gfx::ImageSkia& icon,
      base::OnceClosure done_callback) = 0;

  // Shows a modal dialog to Enhanced Safe Browsing users before the extension
  // install dialog if the extension is not included in the Safe Browsing CRX
  // allowlist.
  virtual void ShowExtensionInstallFrictionDialog(
      content::WebContents* web_contents,
      base::OnceCallback<void(bool)> callback) = 0;

#if BUILDFLAG(IS_ANDROID)
  // Shows a dialog to notify the user that they need to ask their parent for
  // approval to install an extension.
  virtual void ShowExtensionInstallAskParentDialog(
      content::WebContents* web_contents,
      base::OnceClosure cancel_callback,
      base::OnceClosure approve_callback) = 0;
#endif  // BUILDFLAG(IS_ANDROID)

  // Called when the user accepts the extension install friction dialog.
  virtual void ReportFrictionAcceptedEvent(
      content::BrowserContext* context) = 0;

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  // Returns true if safe browsing is enabled and the safe browsing service is
  // present in the embedder.
  virtual bool IsSafeBrowsingEnabledAndReady(
      content::BrowserContext* context) = 0;

  // Returns SafeBrowsingNavigationObserverManager associated with `context`.
  virtual safe_browsing::SafeBrowsingNavigationObserverManager*
  GetSafeBrowsingNavigationObserverManager(
      content::BrowserContext* context) = 0;
#endif

  // Maybe create promotion eligibility checker.
  virtual std::unique_ptr<enterprise_promotion::PromotionEligibilityChecker>
  CreatePromotionEligibilityChecker(content::BrowserContext* context,
                                    bool dismissed_banner_pref,
                                    bool feature_enabled) = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_WEBSTORE_PRIVATE_WEBSTORE_PRIVATE_API_DELEGATE_H_
