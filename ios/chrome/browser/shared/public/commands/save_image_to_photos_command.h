// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_SAVE_IMAGE_TO_PHOTOS_COMMAND_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_SAVE_IMAGE_TO_PHOTOS_COMMAND_H_

#import <Foundation/Foundation.h>

#import <string>

#import "base/memory/weak_ptr.h"
#import "url/origin.h"

class GURL;
namespace web {
struct Referrer;
class WebState;
}  // namespace web

// Contains the data necessary to save an image to a Photos library.
@interface SaveImageToPhotosCommand : NSObject

- (instancetype)initWithImageURL:(GURL)imageURL
                        referrer:(web::Referrer)referrer
                        webState:(web::WebState*)webState
                         frameID:(std::string)frameID
                     frameOrigin:(url::Origin)frameOrigin;

- (instancetype)init NS_UNAVAILABLE;

// The source URL of the image to save to Photos.
@property(nonatomic, assign, readonly) GURL imageURL;

// Referrer of the image in case it needs to be fetched again.
@property(nonatomic, assign, readonly) web::Referrer referrer;

// The web state "containing" the image.
@property(nonatomic, assign, readonly) base::WeakPtr<web::WebState> webState;

// The frame ID where the image resides.
@property(nonatomic, assign, readonly) std::string frameID;

// The origin of the frame where the image resides.
@property(nonatomic, assign, readonly) url::Origin frameOrigin;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_SAVE_IMAGE_TO_PHOTOS_COMMAND_H_
