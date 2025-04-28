// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_HOME_BACKGROUND_CUSTOMIZATION_SERVICE_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_HOME_BACKGROUND_CUSTOMIZATION_SERVICE_H_

#import <string>

#import "base/observer_list.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/sync/protocol/theme_specifics.pb.h"

@class BackgroundCustomizationConfiguration;
class GURL;
class HomeBackgroundCustomizationServiceObserver;

// Service for allowing customization of the Home surface background.
class HomeBackgroundCustomizationService : public KeyedService {
 public:
  explicit HomeBackgroundCustomizationService();

  HomeBackgroundCustomizationService(
      const HomeBackgroundCustomizationService&) = delete;
  HomeBackgroundCustomizationService& operator=(
      const HomeBackgroundCustomizationService&) = delete;

  ~HomeBackgroundCustomizationService() override;

  // KeyedService implementation:
  void Shutdown() override;

  // Returns the current background data.
  const sync_pb::ThemeSpecifics::NtpCustomBackground& GetCurrentBackground();

  /// Sets the background to the given parameters. This represents a background
  /// image url from the NtpBackgroundService.
  /// - `background_url` is the URL of the background itself.
  /// - `thumbnail_url` is the URL of the preview thumbnail.
  /// - `attribution_line_1` is the first line of attribution for the author of
  /// the image.
  /// - `attribution_line_1` is the second line of attribution for the author of
  /// the image.
  /// - `action_url` is an action that can be taken for the attribution (e.g.
  /// visit the artist's webpage).
  /// - `collection_id` is the id of the collection the image comes from.
  void SetCurrentBackground(const GURL& background_url,
                            const GURL& thumbnail_url,
                            const std::string& attribution_line_1,
                            const std::string& attribution_line_2,
                            const GURL& attribution_action_url,
                            const std::string& collection_id);

  // Adds/Removes HomeBackgroundCustomizationServiceObserver observers.
  void AddObserver(HomeBackgroundCustomizationServiceObserver* observer);
  void RemoveObserver(HomeBackgroundCustomizationServiceObserver* observer);

 private:
  // Alerts observers when the background changes.
  void NotifyObserversOfBackgroundChange();

  sync_pb::ThemeSpecifics::NtpCustomBackground current_background_;

  base::ObserverList<HomeBackgroundCustomizationServiceObserver> observers_;
};

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_HOME_BACKGROUND_CUSTOMIZATION_SERVICE_H_
