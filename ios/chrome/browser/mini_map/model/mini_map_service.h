// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_MINI_MAP_MODEL_MINI_MAP_SERVICE_H_
#define IOS_CHROME_BROWSER_MINI_MAP_MODEL_MINI_MAP_SERVICE_H_

#import <UIKit/UIKit.h>

#import "base/memory/raw_ptr.h"
#import "base/no_destructor.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/prefs/pref_member.h"
#import "components/search_engines/template_url_service_observer.h"
#import "components/signin/public/identity_manager/identity_manager.h"

class PrefService;
class TemplateURLService;

// A service to observe Profile scoped MiniMap prefs.
class MiniMapService : public KeyedService,
                       public TemplateURLServiceObserver,
                       public signin::IdentityManager::Observer {
 public:
  MiniMapService(PrefService* pref_service,
                 TemplateURLService* template_url_service,
                 signin::IdentityManager* authentication_service);
  ~MiniMapService() override;

  // Whether the mini map feature is currently enabled.
  bool IsMiniMapEnabled();

  // Whether the current default search engine is Google.
  bool IsDSEGoogle();

  // Whether GoogleMaps is installed.
  bool IsGoogleMapsInstalled();

  // Whether the user is signed in.
  bool IsSignedIn();

  // KeyedService
  void Shutdown() override;

  // TemplateURLServiceObserver
  void OnTemplateURLServiceChanged() override;
  void OnTemplateURLServiceShuttingDown() override;

  // signin::IdentityManager::Observer
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;
  void OnIdentityManagerShutdown(
      signin::IdentityManager* identity_manager) override;

  // Called when `is_mini_map_enabled_` changes (i.e. when the user changes the
  // mini map pref).
  void OnPrefChanged();

  MiniMapService(const MiniMapService&) = delete;
  MiniMapService& operator=(const MiniMapService&) = delete;

 private:
  // Used to observe the preferences of MiniMap
  raw_ptr<PrefService> pref_service_ = nullptr;

  // Service to check if the DSE is Google.
  raw_ptr<TemplateURLService> template_url_service_;
  // Whether the current default search engine is Google.
  bool is_dse_google_ = false;

  // Identity manager to check the sign in status.
  raw_ptr<signin::IdentityManager> identity_manager_;
  // Whether the user is signed in.
  bool is_signed_in_ = false;

  // An object to observe the mini map preference.
  BooleanPrefMember mini_map_enabled_pref_;
  // Whether the mini map feature is currently enabled.
  bool is_mini_map_enabled_ = false;

  // An observer for App scoped data.
  class MiniMapAppObserver {
   public:
    static MiniMapAppObserver* GetInstance();

    // Whether GoogleMaps is installed.
    bool IsGoogleMapsInstalled();

   private:
    friend class base::NoDestructor<MiniMapAppObserver>;

    MiniMapAppObserver();
    ~MiniMapAppObserver();
    // Called when foreground_notification_observer_ (i.e. the application
    // becomes active).
    void OnAppDidBecomeActive();

    // Observer for UIApplicationDidBecomeActiveNotification.
    id foreground_notification_observer_;
    // Whether GoogleMaps is installed.
    bool is_google_maps_installed_ = false;
  };

  base::WeakPtrFactory<MiniMapService> weak_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_MINI_MAP_MODEL_MINI_MAP_SERVICE_H_
