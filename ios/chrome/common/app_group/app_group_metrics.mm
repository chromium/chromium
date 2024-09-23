// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/app_group/app_group_metrics.h"

#import "base/check_op.h"

namespace app_group {

NSString* const kPendingLogFileSuffix = @"_PendingLog";

NSString* const kPendingLogFileDirectory = @"ExtensionLogs";

NSString* const kSearchExtensionDisplayCount = @"SearchExtensionDisplayCount";

NSString* const kContentExtensionDisplayCount = @"ContentExtensionDisplayCount";

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

// To avoid collision between session_ids from chrome or external
// components, the session ID is offset depending on the application.
int AppGroupSessionID(int session_id, AppGroupApplications application) {
  DCHECK_LT(session_id, 1 << 23);
  return (1 << 23) * static_cast<int>(application) + session_id;
}

}  // namespace app_group
