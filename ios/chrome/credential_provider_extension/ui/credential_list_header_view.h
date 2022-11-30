// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_CREDENTIAL_LIST_HEADER_VIEW_H_
#define IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_CREDENTIAL_LIST_HEADER_VIEW_H_

#import <UIKit/UIKit.h>

@interface CredentialListHeaderView : UITableViewHeaderFooterView

// The reuseID for this view.
@property(nonatomic, class, readonly) NSString* reuseID;

// View containing the header text. Exposed so the view controller can set the
// text correctly.
@property(nonatomic, strong) UILabel* headerTextLabel;

@end

#endif  // IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_CREDENTIAL_LIST_HEADER_VIEW_H_
