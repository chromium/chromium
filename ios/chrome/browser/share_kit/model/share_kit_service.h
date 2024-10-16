// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARE_KIT_MODEL_SHARE_KIT_SERVICE_H_
#define IOS_CHROME_BROWSER_SHARE_KIT_MODEL_SHARE_KIT_SERVICE_H_

#import <UIKit/UIKit.h>

#import "base/memory/weak_ptr.h"
#import "components/keyed_service/core/keyed_service.h"

class TabGroup;

// Service for ShareKit, allowing to manage tab groups sharing.
class ShareKitService : public KeyedService {
 public:
  ShareKitService();
  ShareKitService(const ShareKitService&) = delete;
  ShareKitService& operator=(const ShareKitService&) = delete;
  ~ShareKitService() override;

  // Whether the service is supported. This value does not change during the
  // execution of the application.
  virtual bool IsSupported() const = 0;

  // Initiates the share of `group`, presenting a view controller on top of
  // `base_view_controller`.
  virtual void ShareGroup(const TabGroup* group,
                          UIViewController* base_view_controller) = 0;

  // Returns a new FacePile view controller for `collab_id`. It will be a
  // "share" button if `collab_id` is nil.
  // TODO(crbug.com/358373145): Make it pure virtual once downstream is landed.
  virtual UIViewController* FacePile(NSString* collab_id);
};

#endif  // IOS_CHROME_BROWSER_SHARE_KIT_MODEL_SHARE_KIT_SERVICE_H_
