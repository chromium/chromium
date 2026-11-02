// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LOCATION_BAR_UI_BUNDLED_LOCATION_BAR_URL_LOADER_H_
#define IOS_CHROME_BROWSER_LOCATION_BAR_UI_BUNDLED_LOCATION_BAR_URL_LOADER_H_

#import <Foundation/Foundation.h>

#import "components/search_engines/template_url.h"
#import "ui/base/mojom/window_open_disposition.mojom-forward.h"
#import "ui/base/page_transition_types.h"

class GURL;

// A means of loading URLs for the location bar.
@protocol LocationBarURLLoader
- (void)loadGURLFromLocationBar:(const GURL&)url
                               postContent:
                                   (TemplateURLRef::PostContent*)postContent
                                transition:(ui::PageTransition)transition
                               disposition:
                                   (ui::mojom::WindowOpenDisposition)disposition
    destination_url_entered_without_scheme:
        (bool)destination_url_entered_without_scheme;
@end

#endif  // IOS_CHROME_BROWSER_LOCATION_BAR_UI_BUNDLED_LOCATION_BAR_URL_LOADER_H_
