// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AIM_MODEL_AI_MODE_BUTTON_SERVICE_IOS_H_
#define IOS_CHROME_BROWSER_AIM_MODEL_AI_MODE_BUTTON_SERVICE_IOS_H_

#import <UIKit/UIKit.h>

#import "base/callback_list.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/scoped_observation.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/search_engines/template_url_service_observer.h"
#import "url/gurl.h"

class AiModeButtonService;
struct AiModeButtonUiConfig;
class AimEligibilityService;
@class FaviconAttributes;
class FaviconLoader;
class TemplateURLService;

namespace favicon {
class FaviconService;
}  // namespace favicon

namespace gfx {
class Image;
}  // namespace gfx

namespace image_fetcher {
class ImageFetcherService;
struct RequestMetadata;
}  // namespace image_fetcher

// KeyedService providing properties of the AI Mode NTP button on iOS.
class AIModeButtonServiceIOS : public KeyedService,
                               public TemplateURLServiceObserver {
 public:
  // Source of the icon for the AI Mode button.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // See AiModePageActionIconSource in
  // tools/metrics/histograms/metadata/omnibox/enums.xml.
  enum class IconSource {
    kInvisible = 0,
    kVectorIcon = 1,
    kMemoryFaviconCache = 2,
    kDiskDbFaviconCache = 3,
    kNetworkFetch = 4,
    kFailedIcon = 5,
    kMaxValue = kFailedIcon,
  };

  AIModeButtonServiceIOS(
      TemplateURLService* template_url_service,
      AimEligibilityService* aim_eligibility_service,
      AiModeButtonService* ai_mode_button_service,
      FaviconLoader* favicon_loader,
      favicon::FaviconService* favicon_service,
      image_fetcher::ImageFetcherService* image_fetcher_service);
  AIModeButtonServiceIOS(const AIModeButtonServiceIOS&) = delete;
  AIModeButtonServiceIOS& operator=(const AIModeButtonServiceIOS&) = delete;
  ~AIModeButtonServiceIOS() override;

  // KeyedService:
  void Shutdown() override;

  // Whether the AI Mode button is available on the NTP.
  bool IsButtonAvailable() const;

  // Records `Omnibox.AimEntrypoint.Shown` and its `.google` / `.3p` slices.
  void RecordEntrypointShown(bool shown) const;

  // The title for the AI Mode button.
  NSString* GetTitle() const;

  // The accessibility label for the AI Mode button.
  NSString* GetAccessibilityLabel() const;

  // The icon for the AI Mode button.
  UIImage* GetIcon() const;

  // The URL to navigate to when the AI Mode button is tapped.
  GURL GetUrl() const;

  // Registers a callback to be called when the button state (availability,
  // icon, title, etc.) changes.
  base::CallbackListSubscription RegisterStateChangedCallback(
      base::RepeatingClosure callback);

  // TemplateURLServiceObserver:
  void OnTemplateURLServiceChanged() override;
  void OnTemplateURLServiceShuttingDown() override;

 private:
  // Returns the 3P AI mode button UI config if the default search provider is
  // not Google and a valid config is available, or nullptr otherwise.
  const AiModeButtonUiConfig* GetThirdPartyConfig() const;

  // Called when the eligibility service notifies that AIM eligibility changed.
  void OnEligibilityChanged();

  // Called when the AI mode button service notifies that config changed.
  void OnAiModeButtonConfigChanged(const AiModeButtonUiConfig* config);

  // Updates or fetches the 3P favicon for the current DSE configuration.
  void UpdateFavicon();

  // Called when `FaviconLoader` completes a local cache/database lookup.
  void OnFaviconLoaded(const GURL& favicon_url,
                       FaviconAttributes* attributes,
                       bool cached);

  // Fetches the favicon from `favicon_url` over the network via `ImageFetcher`.
  void FetchFaviconFromNetwork(const GURL& favicon_url);

  // Called when `ImageFetcher` finishes downloading the favicon from network.
  void OnFaviconFetchedFromNetwork(
      const GURL& favicon_url,
      const gfx::Image& image,
      const image_fetcher::RequestMetadata& metadata);

  // Notifies all registered state change observers.
  void NotifyStateChanged();

  raw_ptr<TemplateURLService> template_url_service_ = nullptr;
  raw_ptr<AimEligibilityService> aim_eligibility_service_ = nullptr;
  raw_ptr<AiModeButtonService> ai_mode_button_service_ = nullptr;
  raw_ptr<FaviconLoader> favicon_loader_ = nullptr;
  raw_ptr<favicon::FaviconService> favicon_service_ = nullptr;
  raw_ptr<image_fetcher::ImageFetcherService> image_fetcher_service_ = nullptr;

  // The currently loaded 3P favicon image and its source URL.
  UIImage* current_favicon_ = nil;
  GURL current_favicon_url_;

  // True when a favicon was just loaded (synchronously or asynchronously) and
  // observers are being notified, used to avoid double-logging
  // `IconSource::kMemoryFaviconCache` when observers call `GetIcon()`.
  bool newly_loaded_favicon_ = false;

  // True while `UpdateFavicon()` is executing synchronously, used to suppress
  // redundant `NotifyStateChanged()` calls when the caller already notifies.
  bool updating_favicon_ = false;

  base::ScopedObservation<TemplateURLService, TemplateURLServiceObserver>
      template_url_service_observation_{this};
  base::CallbackListSubscription eligibility_subscription_;
  base::CallbackListSubscription ai_mode_button_subscription_;

  base::RepeatingClosureList state_changed_callbacks_;

  base::WeakPtrFactory<AIModeButtonServiceIOS> favicon_fetch_weak_factory_{
      this};
};

#endif  // IOS_CHROME_BROWSER_AIM_MODEL_AI_MODE_BUTTON_SERVICE_IOS_H_
