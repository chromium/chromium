// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OVERLAYS_INFOBAR_MODAL_INFOBAR_MODAL_SUPPORTED_OVERLAY_COORDINATOR_CLASSES_H_
#define IOS_CHROME_BROWSER_UI_OVERLAYS_INFOBAR_MODAL_INFOBAR_MODAL_SUPPORTED_OVERLAY_COORDINATOR_CLASSES_H_

#import <Foundation/Foundation.h>

namespace infobar_modal {

// Returns the supported OverlayRequestCoordinator classes for
// OverlayModality::kInfobarModal.
NSArray<Class>* GetSupportedOverlayCoordinatorClasses();

}  // infobar_modal

#endif  // IOS_CHROME_BROWSER_UI_OVERLAYS_INFOBAR_MODAL_INFOBAR_MODAL_SUPPORTED_OVERLAY_COORDINATOR_CLASSES_H_
