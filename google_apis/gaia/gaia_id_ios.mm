// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "google_apis/gaia/gaia_id.h"

#import <Foundation/Foundation.h>

#import "base/strings/sys_string_conversions.h"

GaiaId::GaiaId(NSString* value) : id_(base::SysNSStringToUTF8(value)) {}

NSString* GaiaId::ToNSString() const {
  return base::SysUTF8ToNSString(id_);
}
