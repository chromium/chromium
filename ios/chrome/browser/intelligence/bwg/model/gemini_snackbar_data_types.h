// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_SNACKBAR_DATA_TYPES_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_SNACKBAR_DATA_TYPES_H_

#import <Foundation/Foundation.h>

// Semantic type of a snackbar requested by the Gemini SDK.
// Keep in sync with `GCRGeminiSnackbarType`.
enum class GeminiSnackbarType {
  // A generic snackbar that displays the provided localized message.
  kGeneric = 0,
  // Displayed when there is no internet connection.
  kNoInternet,
  // Displayed for screen or tab context attachment status or errors.
  kContextAttachment,
  // Displayed when an attached image exceeds count or size limits.
  kImageAttachmentError,
  // Displayed for image actions (e.g., saved to Photos, copied to clipboard, or
  // failure).
  kImageAction,
  // Displayed when a response is copied to the clipboard.
  kResponseCopied,
  // Displayed when the image generation (Nano Banana) quota is exceeded.
  kNanoBananaQuotaExceeded,
  // Displayed when Gemini Live encounters an error.
  kLiveError,
  // Displayed for a server-driven operation (e.g., 1P tool confirmation with an
  // Undo/Open action).
  kServerDriven,
  // Displayed during public link creation or when public link creation fails.
  kPublicLinkSharing,
};

// Represents a snackbar presentation request from the Gemini SDK.
@interface GeminiSnackbar : NSObject

// The semantic type of the snackbar.
@property(nonatomic, readonly, assign) GeminiSnackbarType type;

// The localized message text to display in the snackbar.
@property(nonatomic, readonly, copy) NSString* message;

// Optional localized title for an action button on the snackbar.
@property(nonatomic, readonly, copy) NSString* actionTitle;

// Optional handler invoked when the user taps the snackbar's action button.
@property(nonatomic, readonly, copy) void (^actionHandler)(void);

- (instancetype)initWithType:(GeminiSnackbarType)type
                     message:(NSString*)message
                 actionTitle:(NSString*)actionTitle
               actionHandler:(void (^)(void))actionHandler
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithType:(GeminiSnackbarType)type
                     message:(NSString*)message;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_SNACKBAR_DATA_TYPES_H_
