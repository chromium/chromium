// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/downloads/save_to_photos/fake_save_to_photos_settings_mutator.h"

#import "base/check.h"
#import "google_apis/gaia/gaia_id.h"

@implementation FakeSaveToPhotosSettingsMutator {
  GaiaId _selectedIdentityGaiaID;
}

- (void)setSelectedIdentityGaiaID:(const GaiaId*)gaiaID {
  DCHECK(gaiaID);
  _selectedIdentityGaiaID = *gaiaID;
}

- (GaiaId)selectedIdentityGaiaID {
  return _selectedIdentityGaiaID;
}

@end
