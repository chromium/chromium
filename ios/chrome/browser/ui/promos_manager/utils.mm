// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/promos_manager/utils.h"

#import "ios/chrome/browser/shared/model/application_context/application_context.h"

bool ShouldDisplayPromos() {
  return GetApplicationContext()->WasLastShutdownClean();
}
