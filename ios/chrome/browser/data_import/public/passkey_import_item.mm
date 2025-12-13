// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/data_import/public/passkey_import_item.h"

#import "base/strings/sys_string_conversions.h"
#import "components/webauthn/core/browser/import/import_processing_result.h"

@implementation PasskeyImportItem

+ (NSArray<PasskeyImportItem*>*)passkeyImportItemsFromImportedPasskeyInfos:
    (const std::vector<webauthn::ImportedPasskeyInfo>&)passkeyInfos {
  NSMutableArray<PasskeyImportItem*>* passkeyItems =
      [NSMutableArray arrayWithCapacity:passkeyInfos.size()];
  for (const webauthn::ImportedPasskeyInfo& passkeyInfo : passkeyInfos) {
    PasskeyImportItem* item = [[PasskeyImportItem alloc]
        initWithRpId:base::SysUTF8ToNSString(passkeyInfo.rp_id)
            username:base::SysUTF8ToNSString(passkeyInfo.user_name)];
    [passkeyItems addObject:item];
  }
  return passkeyItems;
}

- (instancetype)initWithRpId:(NSString*)rpId username:(NSString*)username {
  self = [super init];
  if (self) {
    _rpId = rpId;
    _username = username;
  }
  return self;
}

@end
