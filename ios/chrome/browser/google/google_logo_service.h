// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_GOOGLE_GOOGLE_LOGO_SERVICE_H_
#define IOS_CHROME_BROWSER_GOOGLE_GOOGLE_LOGO_SERVICE_H_

#include <memory>

#include "components/search_provider_logos/logo_common.h"
#include "components/search_provider_logos/logo_service_impl.h"
#include "third_party/skia/include/core/SkBitmap.h"

// Provides the logo if a BrowserState's default search provider is Google.
// In addition to the GetLogo() method provided by the base implementation,
// includes extra methods {Get,Set}CachedLogo() as an extra, nearer cache.
//
// Example usage:
//   GoogleLogoService* logo_service =
//       GoogleLogoServiceFactory::GetForBrowserState(browser_state);
//   logo_service->GetLogo(...);
//
class GoogleLogoService : public search_provider_logos::LogoServiceImpl {
 public:
  GoogleLogoService(
      TemplateURLService* template_url_service,
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~GoogleLogoService() override;

  using LogoServiceImpl::GetLogo;

  // |LogoService::GetLogo| does everything on callbacks, and iOS needs to load
  // the logo immediately on page load. This caches the SkBitmap so we can
  // immediately load. This prevents showing the google logo on every new tab
  // page and immediately animating to the logo. Only one SkBitmap is cached per
  // BrowserState.
  void SetCachedLogo(const search_provider_logos::Logo* logo);
  search_provider_logos::Logo GetCachedLogo();

 private:
  SkBitmap cached_image_;
  search_provider_logos::LogoMetadata cached_metadata_;
  const search_provider_logos::LogoMetadata empty_metadata;

  DISALLOW_COPY_AND_ASSIGN(GoogleLogoService);
};

#endif  // IOS_CHROME_BROWSER_GOOGLE_GOOGLE_LOGO_SERVICE_H_
