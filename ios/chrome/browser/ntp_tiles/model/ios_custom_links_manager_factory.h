// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_TILES_MODEL_IOS_CUSTOM_LINKS_MANAGER_FACTORY_H_
#define IOS_CHROME_BROWSER_NTP_TILES_MODEL_IOS_CUSTOM_LINKS_MANAGER_FACTORY_H_

#import <memory>

class ProfileIOS;

namespace ntp_tiles {
class CustomLinksManager;
}

class IOSCustomLinksManagerFactory {
 public:
  static std::unique_ptr<ntp_tiles::CustomLinksManager> NewForProfile(
      ProfileIOS* profile);
};

#endif  // IOS_CHROME_BROWSER_NTP_TILES_MODEL_IOS_CUSTOM_LINKS_MANAGER_FACTORY_H_
