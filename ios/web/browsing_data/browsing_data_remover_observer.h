// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_BROWSING_DATA_BROWSING_DATA_REMOVER_OBSERVER_H_
#define IOS_WEB_BROWSING_DATA_BROWSING_DATA_REMOVER_OBSERVER_H_

#import <Foundation/Foundation.h>

namespace web {
class BrowsingDataRemover;
}

// Protocol used to observe the BrowsingDataRemover.
@protocol BrowsingDataRemoverObserver

// Called when the |dataRemover| is about to remove browsing data.
- (void)willRemoveBrowsingData:(web::BrowsingDataRemover*)dataRemover;

// Called when the |dataRemover| has finished removing browsing data.
- (void)didRemoveBrowsingData:(web::BrowsingDataRemover*)dataRemover;

@end

#endif  // IOS_WEB_BROWSING_DATA_BROWSING_DATA_REMOVER_OBSERVER_H_
