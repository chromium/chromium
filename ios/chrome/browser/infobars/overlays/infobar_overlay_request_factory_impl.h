// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_INFOBAR_OVERLAY_REQUEST_FACTORY_IMPL_H_
#define IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_INFOBAR_OVERLAY_REQUEST_FACTORY_IMPL_H_

#include <map>
#include <vector>

#include "components/infobars/core/infobar_delegate.h"
#import "ios/chrome/browser/infobars/infobar_type.h"
#import "ios/chrome/browser/infobars/overlays/infobar_overlay_request_factory.h"
#include "ios/chrome/browser/overlays/public/overlay_request.h"

class InfoBarIOS;

// Implementation of InfobarOverlayRequestFactory.
class InfobarOverlayRequestFactoryImpl : public InfobarOverlayRequestFactory {
 public:
  explicit InfobarOverlayRequestFactoryImpl();
  ~InfobarOverlayRequestFactoryImpl() override;

 private:
  // InfobarOverlayRequestFactory:
  std::unique_ptr<OverlayRequest> CreateInfobarRequest(
      infobars::InfoBar* infobar,
      InfobarOverlayType type) override;

  // Helper object used to create OverlayRequests.  Subclasses should be created
  // for each InfobarType and InfobarOverlayType.
  class FactoryHelper {
   public:
    FactoryHelper() = default;
    virtual ~FactoryHelper() = default;

    virtual std::unique_ptr<OverlayRequest> CreateInfobarRequest(
        InfoBarIOS* infobar) const = 0;
  };

  // Template for a helper objects used to create OverlayRequests.
  // Specializations of this template should be created for each
  // InfobarType and InfobarOverlayType.
  template <class RequestConfigType>
  class FactoryHelperImpl : public FactoryHelper {
   public:
    FactoryHelperImpl() = default;
    virtual ~FactoryHelperImpl() override = default;

    // CreationHelperBase:
    std::unique_ptr<OverlayRequest> CreateInfobarRequest(
        InfoBarIOS* infobar) const override {
      return OverlayRequest::CreateWithConfig<RequestConfigType>(infobar);
    }
  };

  // Storage object that holds the factory helpers for each InfobarOverlayType
  // for a given InfobarType.
  class FactoryHelperStorage {
   public:
    FactoryHelperStorage();
    FactoryHelperStorage(std::unique_ptr<FactoryHelper> banner_factory,
                         std::unique_ptr<FactoryHelper> modal_factory);
    FactoryHelperStorage(FactoryHelperStorage&& storage);
    ~FactoryHelperStorage();

    // Returns the factory for |type|.
    FactoryHelper* operator[](InfobarOverlayType type);

   private:
    // The factory helper for each InfobarOverlayType.
    std::map<InfobarOverlayType, std::unique_ptr<FactoryHelper>> factories_;
  };

  // Creates a FactoryHelper that creates OverlayRequests using ConfigType.
  template <class ConfigType>
  std::unique_ptr<FactoryHelper> CreateFactory() {
    return std::make_unique<FactoryHelperImpl<ConfigType>>();
  }

  // Creates a FactoryHelperStorage with the passed factory helpers, then adds
  // it to |factory_storages_|.
  void SetUpFactories(InfobarType type,
                      std::unique_ptr<FactoryHelper> banner_factory,
                      std::unique_ptr<FactoryHelper> modal_factory);

  // Map containing the factory storages for each of InfobarType.
  std::map<InfobarType, FactoryHelperStorage> factory_storages_;
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_INFOBAR_OVERLAY_REQUEST_FACTORY_IMPL_H_
