// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_STATE_LENS_OVERLAY_STATE_NOTIFIER_H_
#define IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_STATE_LENS_OVERLAY_STATE_NOTIFIER_H_

#import <Foundation/Foundation.h>

@class LensOverlayStateNotifier;

// Protocol for observers of the lens overlay events.
@protocol LensOverlayStateNotifierObserver <NSObject>

@optional

// Notifies observers that Lens Overlay was prepared.
- (void)lensOverlayDidPrepare:
    (LensOverlayStateNotifier*)lensOverlayStateNotifier;

// Notifies observers that Lens Overlay is about to appear.
- (void)lensOverlayWillAppear:
    (LensOverlayStateNotifier*)lensOverlayStateNotifier;

// Notifies observers that Lens Overlay is about to disappear.
- (void)lensOverlayWillDisappear:
    (LensOverlayStateNotifier*)lensOverlayStateNotifier;

// Notifies observers that Lens Overlay did disappear.
- (void)lensOverlayDidDisappear:
    (LensOverlayStateNotifier*)lensOverlayStateNotifier;

// Notifies observers that Lens Overlay readjusted its presentation.
- (void)lensOverlayDidReadjustPresentation:
    (LensOverlayStateNotifier*)lensOverlayStateNotifier;

@end

// Object used to notify observer about events for the Lens overlay.
// *****
// NOTE: this is an experimental object. Don't take it as example.
// *****
@interface LensOverlayStateNotifier : NSObject

// Notifies observers that Lens Overlay was prepared.
- (void)lensOverlayDidPrepare;

// Notifies observers that Lens Overlay is about to appear.
- (void)lensOverlayWillAppear;

// Notifies observers that Lens Overlay is about to disappear.
- (void)lensOverlayWillDisappear;

// Notifies observers that Lens Overlay did disappear.
- (void)lensOverlayDidDisappear;

// Notifies observers that Lens Overlay readjusted its presentation.
- (void)lensOverlayDidReadjustPresentation;

// Adds an observer to be notified of lens overlay state changes.
- (void)addObserver:(id<LensOverlayStateNotifierObserver>)observer;

// Removes a previously added observer.
- (void)removeObserver:(id<LensOverlayStateNotifierObserver>)observer;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_STATE_LENS_OVERLAY_STATE_NOTIFIER_H_
