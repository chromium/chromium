// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef IOS_WEB_PUBLIC_UI_CONTEXT_MENU_PARAMS_H_
#define IOS_WEB_PUBLIC_UI_CONTEXT_MENU_PARAMS_H_

#import <UIKit/UIKit.h>

#include <string>

#include "base/strings/string16.h"
#include "ios/web/public/navigation/referrer.h"
#include "url/gurl.h"

namespace web {

// Enum for identifying how the menu title was constructed.
enum class ContextMenuTitleOrigin {
  kUnknown = 0,
  kURL = 1,           // the menu title is a URL (href or image src).
  kImageTitle = 2,    // the menu title is an image's title text
  kImageAltText = 3,  // the menu title is an image's alt text and src
};

// Wraps information needed to show a context menu.
struct ContextMenuParams {
 public:
  ContextMenuParams();
  ContextMenuParams(const ContextMenuParams& other);
  ~ContextMenuParams();

  // Whether or not the context menu was triggered from the main frame.
  bool is_main_frame;

  // The title of the menu.
  NSString* menu_title;

  // How the menu title was constructed.
  ContextMenuTitleOrigin menu_title_origin;

  // The URL of the link that encloses the node the context menu was invoked on.
  GURL link_url;

  // The source URL of the element the context menu was invoked on. Example of
  // elements with source URLs are img, audio, and video.
  GURL src_url;

  // The referrer policy to use when opening the link.
  web::ReferrerPolicy referrer_policy;

  // The view in which to present the menu.
  UIView* view;

  // The location in |view| to present the menu.
  CGPoint location;

  // The text associated with the link. It is either nil or nonempty (it can not
  // be empty).
  NSString* link_text;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_UI_CONTEXT_MENU_PARAMS_H_
