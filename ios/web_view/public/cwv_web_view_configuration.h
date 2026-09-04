// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_WEB_VIEW_CONFIGURATION_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_WEB_VIEW_CONFIGURATION_H_

#import <Foundation/Foundation.h>

#import "cwv_export.h"

NS_ASSUME_NONNULL_BEGIN

@class CWVAutofillDataManager;
@class CWVPreferences;
@class CWVLeakCheckService;
@class CWVReuseCheckService;
@class CWVSyncController;
@class CWVUserContentController;
@class CWVWebsiteDataStore;

// Configuration used for creation of a CWVWebView.
CWV_EXPORT
@interface CWVWebViewConfiguration : NSObject

// Configuration with persistent data store which stores all data on disk.
// Every call returns the same instance.
+ (instancetype)defaultConfiguration;

// Configuration with ephemeral data store that never stores data on disk.
// Every call returns the same instance.
// Deprecated. Use |nonPersistentConfiguration| instead.
+ (instancetype)incognitoConfiguration;

// Configuration with non-persistent data store that never stores data on disk.
// Every call returns a new instance.
+ (instancetype)nonPersistentConfiguration;

/**
 * Returns a cached persistent configuration for the given storage identifier.
 * Configurations with the same identifier share the same
 * data store and preferences.
 *
 * @param storageIdentifier A unique identifier.
 * @return The configuration instance.
 */
+ (instancetype)configurationWithIdentifier:(NSUUID*)storageIdentifier
    API_AVAILABLE(ios(17.0));

/**
 * Shuts down the BrowserState and asynchronously deletes all on-disk data
 * associated with this configuration. Should be called when the embedding
 * application wants to remove all data for this configuration.
 *
 * @note This method will abort cleanup if there are active CWVWebViews using
 * this configuration; this condition will trigger a CHECK.
 */
- (void)remove API_AVAILABLE(ios(17.0));

- (instancetype)init NS_UNAVAILABLE;

// The preferences object associated with this web view configuration.
@property(nonatomic, readonly) CWVPreferences* preferences;

// The storage identifier associated with this configuration, or nil for default
// or non-persistent configurations.
@property(nonatomic, readonly, copy, nullable)
    NSUUID* storageIdentifier API_AVAILABLE(ios(17.0));

// The user content controller to associate with web views created using this
// configuration.
@property(nonatomic, readonly) CWVUserContentController* userContentController;

// This web view configuration's sync controller.
// nil if -[CWVWebViewConfiguration isPersistent] is NO.
@property(nonatomic, readonly, nullable) CWVSyncController* syncController;

// This web view configuration's autofill data manager.
// nil if -[CWVWebViewConfiguration isPersistent] is NO.
@property(nonatomic, readonly, nullable)
    CWVAutofillDataManager* autofillDataManager;

// This web view configuration's leak check service.
// nil if -[CWVWebViewConfiguration isPersistent] is NO.
@property(nonatomic, readonly, nullable) CWVLeakCheckService* leakCheckService;

// This web view configuration's reuse check utility.
// nil if -[CWVWebViewConfiguration isPersistent] is NO.
@property(nonatomic, readonly, nullable)
    CWVReuseCheckService* reuseCheckService;

// YES if this is a configuration with a persistent data store which stores all
// data on disk, for example cookies.
@property(nonatomic, readonly, getter=isPersistent) BOOL persistent;

// Returns YES if there are any active CWVWebView instances currently using
// this configuration.
@property(nonatomic, readonly) BOOL hasActiveWebViews;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_WEB_VIEW_CONFIGURATION_H_
