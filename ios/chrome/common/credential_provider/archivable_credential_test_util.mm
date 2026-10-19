// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/credential_provider/archivable_credential_test_util.h"

#import "ios/chrome/common/credential_provider/archivable_credential.h"

NSData* TestDataFromString(std::string_view str) {
  return [NSData dataWithBytes:str.data() length:str.size()];
}

ArchivableCredential* TestPasskeyCredential(
    const TestPasskeyCredentialOverrides& overrides) {
  return [[ArchivableCredential alloc]
              initWithFavicon:overrides.favicon
                         gaia:overrides.gaia
             recordIdentifier:overrides.record_identifier
                       syncId:overrides.sync_id
                     username:overrides.username
              userDisplayName:overrides.user_display_name
                       userId:overrides.user_id
                 credentialId:overrides.credential_id
                         rpId:overrides.rp_id
                   privateKey:overrides.private_key
                    encrypted:overrides.encrypted
      securityDomainEncrypted:overrides.security_domain_encrypted
         deviceAuthKeyVersion:overrides.device_auth_key_version
                 creationTime:overrides.creation_time
                 lastUsedTime:overrides.last_used_time
                       hidden:overrides.hidden
                   hiddenTime:overrides.hidden_time
                 editedByUser:overrides.edited_by_user];
}
