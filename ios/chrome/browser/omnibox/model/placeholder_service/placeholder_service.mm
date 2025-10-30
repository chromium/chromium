// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/model/placeholder_service/placeholder_service.h"

#import "base/functional/callback_helpers.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "components/omnibox/common/omnibox_features.h"
#import "components/search_engines/template_url.h"
#import "components/search_engines/template_url_service.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {
// Cooldown period before re-fetching a failed icon.
const base::TimeDelta kFetchCooldown = base::Seconds(2);
}  // namespace

PlaceholderService::PlaceholderService(FaviconLoader* favicon_loader,
                                       TemplateURLService* template_url_service)
    : favicon_loader_(favicon_loader),
      template_url_service_(template_url_service),
      current_dse_(nullptr),
      icon_callbacks_() {
  template_url_service->AddObserver(this);
  icon_cache_ = [[NSCache alloc] init];
}

PlaceholderService::~PlaceholderService() {
  for (auto& observer : model_observers_) {
    observer.OnPlaceholderServiceShuttingDown();
  }
  template_url_service_->RemoveObserver(this);
  icon_cache_ = nil;
}

void PlaceholderService::FetchDefaultSearchEngineIcon(
    CGFloat icon_point_size,
    PlaceholderImageCallback callback) {
  // Return the cached image if there is one.
  UIImage* cached_icon = [icon_cache_ objectForKey:@(icon_point_size)];
  if (cached_icon) {
    if (callback) {
      callback.Run(cached_icon);
    }
    return;
  }

  // Return the placeholder icon if there is no default search provider.
  UIImage* placeholder_icon =
      DefaultSymbolWithPointSize(kSearchSymbol, icon_point_size);
  const TemplateURL* default_provider =
      template_url_service_ ? template_url_service_->GetDefaultSearchProvider()
                            : nullptr;
  if (!default_provider) {
    if (callback) {
      callback.Run(placeholder_icon);
    }
    return;
  }

  // Return the bundled icon if there is one. Also cache it.
  UIImage* bundled_icon =
      GetBundledIconForTemplateURL(default_provider, icon_point_size);
  if (bundled_icon) {
    [icon_cache_ setObject:bundled_icon forKey:@(icon_point_size)];
    if (callback) {
      callback.Run(bundled_icon);
    }
    return;
  }

  // Set a placeholder before fetching the icon.
  if (callback) {
    callback.Run(placeholder_icon);
  }
  // The original `callback` is moved and will be invoked when the fetch
  // completes.
  icon_callbacks_[icon_point_size].push_back(std::move(callback));
  PerformIconFetch(default_provider, icon_point_size);
}

UIImage* PlaceholderService::GetDefaultSearchEngineIcon(
    CGFloat icon_point_size) {
  // Return the cached image if there is one.
  UIImage* cached_icon = [icon_cache_ objectForKey:@(icon_point_size)];
  if (cached_icon) {
    return cached_icon;
  }

  // Return the placeholder icon if there is no default search provider.
  UIImage* placeholder_icon =
      DefaultSymbolWithPointSize(kSearchSymbol, icon_point_size);
  const TemplateURL* default_provider =
      template_url_service_ ? template_url_service_->GetDefaultSearchProvider()
                            : nullptr;
  if (!default_provider) {
    return placeholder_icon;
  }
  // Fetch the icon after return.
  base::ScopedClosureRunner run_after_return =
      base::ScopedClosureRunner(base::BindOnce(
          &PlaceholderService::FetchDefaultSearchEngineIcon,
          base::Unretained(this), icon_point_size, base::DoNothing()));
  return placeholder_icon;
}

NSString* PlaceholderService::GetCurrentPlaceholderText() {
  if (!base::FeatureList::IsEnabled(omnibox::kOmniboxMobileParityUpdate)) {
    return l10n_util::GetNSString(IDS_OMNIBOX_EMPTY_HINT);
  }

  CHECK(template_url_service_);

  std::u16string provider_name = u"";
  if (const TemplateURL* search_provider =
          template_url_service_->GetDefaultSearchProvider()) {
    provider_name = search_provider->short_name();
  }

  return l10n_util::GetNSStringF(IDS_OMNIBOX_EMPTY_HINT_WITH_DSE_NAME,
                                 provider_name);
}

NSString* PlaceholderService::GetCurrentSearchOnlyPlaceholderText() {
  CHECK(template_url_service_);

  std::u16string provider_name = u"";
  if (const TemplateURL* search_provider =
          template_url_service_->GetDefaultSearchProvider()) {
    provider_name = search_provider->short_name();
  }

  return l10n_util::GetNSStringF(IDS_IOS_OMNIBOX_PLACEHOLDER_SEARCH_ONLY,
                                 provider_name);
}

base::WeakPtr<PlaceholderService> PlaceholderService::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void PlaceholderService::AddObserver(PlaceholderServiceObserver* observer) {
  model_observers_.AddObserver(observer);
}

void PlaceholderService::RemoveObserver(PlaceholderServiceObserver* observer) {
  model_observers_.RemoveObserver(observer);
}

#pragma mark - TemplateURLServiceObserver

void PlaceholderService::OnTemplateURLServiceChanged() {
  if (!template_url_service_ ||
      template_url_service_->GetDefaultSearchProvider() == current_dse_) {
    return;
  }
  current_dse_ = template_url_service_->GetDefaultSearchProvider();
  [icon_cache_ removeAllObjects];
  icon_callbacks_.clear();
  fetch_cooldowns_.clear();
  for (auto& observer : model_observers_) {
    observer.OnPlaceholderTextChanged();
    observer.OnPlaceholderImageChanged();
  }
}

#pragma mark - Private

UIImage* PlaceholderService::GetBundledIconForTemplateURL(
    const TemplateURL* template_url,
    CGFloat icon_point_size) {
  CHECK(template_url);
  CHECK(template_url_service_);

  // Google bundled icon.
  if (template_url->GetEngineType(template_url_service_->search_terms_data()) ==
      SEARCH_ENGINE_GOOGLE) {
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
    return MakeSymbolMulticolor(
        CustomSymbolWithPointSize(kGoogleIconSymbol, icon_point_size));
#endif
  }
  return nil;
}

void PlaceholderService::OnIconReceivedForTemplateURL(
    TemplateURLID template_url_id,
    CGFloat icon_point_size,
    UIImage* icon) {
  if (icon && template_url_service_ &&
      template_url_service_->GetDefaultSearchProvider()->id() ==
          template_url_id &&
      ![icon_cache_ objectForKey:@(icon_point_size)]) {
    [icon_cache_ setObject:icon forKey:@(icon_point_size)];

    for (auto& observer : model_observers_) {
      observer.OnPlaceholderImageChanged();
    }

    if (icon_callbacks_.contains(icon_point_size)) {
      std::vector<PlaceholderImageCallback> callbacks =
          std::move(icon_callbacks_[icon_point_size]);
      icon_callbacks_.erase(icon_point_size);
      for (PlaceholderImageCallback& callback : callbacks) {
        if (callback) {
          std::move(callback).Run(icon);
        }
      }
    }
  }
}

void PlaceholderService::PerformIconFetch(const TemplateURL* template_url,
                                          CGFloat icon_point_size) {
  if (!favicon_loader_) {
    return;
  }

  // Do not fetch if the cooldown is still active.
  auto it = fetch_cooldowns_.find(icon_point_size);
  if (it != fetch_cooldowns_.end() && base::TimeTicks::Now() < it->second) {
    return;
  }

  // Set a cooldown to prevent rapid refetching.
  fetch_cooldowns_[icon_point_size] = base::TimeTicks::Now() + kFetchCooldown;

  auto favicon_completion = base::CallbackToBlock(base::BindRepeating(
      [](base::WeakPtr<PlaceholderService> weak_self,
         TemplateURLID template_url_id, CGFloat icon_point_size,
         FaviconAttributes* favicon_result, bool cached) {
        if (!favicon_result.faviconImage) {
          return;
        }
        UIImage* favicon = favicon_result.faviconImage;
        if (weak_self) {
          weak_self->OnIconReceivedForTemplateURL(template_url_id,
                                                  icon_point_size, favicon);
        }
      },
      AsWeakPtr(), template_url->id(), icon_point_size));

  // Prepopulated search engines don't have a favicon URL, so the favicon is
  // loaded with an empty query search page URL.
  if (template_url->prepopulate_id() != 0) {
    // Fake up a page URL for favicons of prepopulated search engines, since
    // favicons may be fetched from Google server which doesn't suppoprt icon
    // URL.
    std::string empty_page_url = template_url->url_ref().ReplaceSearchTerms(
        TemplateURLRef::SearchTermsArgs(std::u16string()),
        template_url_service_->search_terms_data());
    favicon_loader_->FaviconForPageUrlOrHost(
        GURL(empty_page_url), icon_point_size, favicon_completion);
  } else {
    // Download the favicon.
    favicon_loader_->FaviconForIconUrl(template_url->favicon_url(),
                                       icon_point_size, icon_point_size,
                                       favicon_completion);
  }
}
