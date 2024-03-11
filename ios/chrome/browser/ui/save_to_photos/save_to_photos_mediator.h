// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SAVE_TO_PHOTOS_SAVE_TO_PHOTOS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_SAVE_TO_PHOTOS_SAVE_TO_PHOTOS_MEDIATOR_H_

#import <Foundation/Foundation.h>

@protocol ApplicationCommands;
class ChromeAccountManagerService;
class GURL;
@protocol ManageStorageAlertCommands;
class PhotosService;
class PrefService;
@protocol SaveToPhotosMediatorDelegate;
@protocol SnackbarCommands;
@protocol SystemIdentity;

namespace signin {
class IdentityManager;
}  // namespace signin

namespace web {
struct Referrer;
class WebState;
}  // namespace web

// Photos app product identifier passed to the StoreKit view.
extern NSString* const kGooglePhotosAppProductIdentifier;

// This provider token is passed to the StoreKit view so the number of Photos
// app installs from Chrome can be measured. This should not be modified.
extern NSString* const kGooglePhotosStoreKitProviderToken;

// This campaign token is passed to the StoreKit view so the number of Photos
// app installs from Chrome can be measured. This should not be modified.
extern NSString* const kGooglePhotosStoreKitCampaignToken;

// Photos app URL template to open the "Recently added" view.
extern NSString* const kGooglePhotosRecentlyAddedURLString;

// Photos app URL scheme to check if the Photos app is installed.
extern NSString* const kGooglePhotosAppURLScheme;

// Save to Photos mediator. This UI is presented when the user has selected an
// image to be saved in their Photos library e.g. Google Photos. It lets the
// user select an account as destination and notifies the user of progress and
// completion.
@interface SaveToPhotosMediator : NSObject

@property(nonatomic, weak) id<SaveToPhotosMediatorDelegate> delegate;

// Initialization.
- (instancetype)initWithPhotosService:(PhotosService*)photosService
                          prefService:(PrefService*)prefService
                accountManagerService:
                    (ChromeAccountManagerService*)accountManagerService
                      identityManager:(signin::IdentityManager*)identityManager
            manageStorageAlertHandler:
                (id<ManageStorageAlertCommands>)manageStorageAlertHandler
                   applicationHandler:
                       (id<ApplicationCommands>)applicationHandler;
- (instancetype)init NS_UNAVAILABLE;

// Starts the process of saving the image.
// - 1. It fetches the image from the given `webState` using `imageURL` and
// `referrer`.
// - 2. If the image has been fetched, it detects whether a default account
// exists for saving to Photos.
// - 3. a. If so, the image is uploaded to the provided `photosService` using
// that default account.
//      b. Otherwise, the account picker is presented through the `delegate`.
- (void)startWithImageURL:(const GURL&)imageURL
                 referrer:(const web::Referrer&)referrer
                 webState:(web::WebState*)webState;

// Resumes the process of saving the image with the given `identity`. If
// `askEveryTime` is NO, then the Gaia ID of `identity` will be memorized so the
// account picker can be skipped next time the user saves an image.
- (void)accountPickerDidSelectIdentity:(id<SystemIdentity>)identity
                          askEveryTime:(BOOL)askEveryTime;

// Called when the user taps "Cancel" in the account picker.
- (void)accountPickerDidCancel;

// Lets the mediator know that the account picker is done being dismissed.
- (void)accountPickerWasHidden;

// Lets the mediator know that the StoreKit view has been dismissed by the user.
// The mediator will prepare to shutdown and ask the delegate to hide Save to
// Photos.
- (void)storeKitWantsToHide;

// Disconnect the mediator from services.
- (void)disconnect;

// Shows the "Manage Storage" web page for `identity` in a new tab.
- (void)showManageStorageForIdentity:(id<SystemIdentity>)identity;

// Called when the user taps "Cancel" in the "Manage Storage" alert.
- (void)manageStorageAlertDidCancel;

@end

#endif  // IOS_CHROME_BROWSER_UI_SAVE_TO_PHOTOS_SAVE_TO_PHOTOS_MEDIATOR_H_
