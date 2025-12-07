// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/app_group/app_group_metrics.h"

#import "base/check_op.h"

namespace app_group {

NSString* const kCredentialExtensionDisplayCount =
    @"CredentialExtensionDisplayCount";

NSString* const kCredentialExtensionReauthCount =
    @"CredentialExtensionReauthCount";

NSString* const kCredentialExtensionCopyURLCount =
    @"CredentialExtensionCopyURLCount";

NSString* const kCredentialExtensionCopyUsernameCount =
    @"CredentialExtensionCopyUsernameCount";

NSString* const kCredentialExtensionCopyUserDisplayNameCount =
    @"CredentialExtensionCopyUserDisplayNameCount";

NSString* const kCredentialExtensionCopyCreationDateCount =
    @"CredentialExtensionCopyCreationDateCount";

NSString* const kCredentialExtensionCopyPasswordCount =
    @"CredentialExtensionCopyPasswordCount";

NSString* const kCredentialExtensionShowPasswordCount =
    @"CredentialExtensionShowPasswordCount";

NSString* const kCredentialExtensionSearchCount =
    @"CredentialExtensionSearchCount";

NSString* const kCredentialExtensionPasswordUseCount =
    @"CredentialExtensionPasswordUseCount";

NSString* const kCredentialExtensionPasskeyUseCount =
    @"CredentialExtensionPasskeyUseCount";

NSString* const kCredentialExtensionQuickPasswordUseCount =
    @"CredentialExtensionQuickPasswordUseCount";

NSString* const kCredentialExtensionQuickPasskeyUseCount =
    @"CredentialExtensionQuickPasskeyUseCount";

NSString* const kCredentialExtensionFetchPasswordFailureCount =
    @"CredentialExtensionFetchPasswordFailureCount";

NSString* const kCredentialExtensionFetchPasswordNilArgumentCount =
    @"CredentialExtensionFetchPasswordNilArgumentCount";

NSString* const kCredentialExtensionKeychainSavePasswordFailureCount =
    @"CredentialExtensionKeychainSavePasswordFailureCount";

NSString* const kCredentialExtensionSaveCredentialFailureCount =
    @"CredentialExtensionSaveCredentialFailureCount";

NSString* HistogramCountKey(NSString* histogram, int bucket) {
  return [NSString stringWithFormat:@"%@.%i", histogram, bucket];
}

}  // namespace app_group
