// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/util/identity_snackbar/identity_snackbar_message.h"

#import "base/check.h"
#import "base/metrics/histogram_functions.h"
#import "base/time/time.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/shared/ui/util/identity_snackbar/identity_snackbar_message_view.h"

@interface IdentitySnackbarMessage ()
// Set the properties as readwrite.
@property(nonatomic, readwrite) UIImage* avatar;
@property(nonatomic, readwrite) NSString* name;
@property(nonatomic, readwrite) NSString* email;
@property(nonatomic, readwrite) BOOL managed;
@end

namespace {
// Name of the histogram recording whether the identity snackbar had a name to
// display.
const char kIdentitySnackbarHadUserName[] =
    "Signin.IdentitySnackbarHadUserName";
}  // namespace

@implementation IdentitySnackbarMessage

- (instancetype)initWithName:(NSString*)name
                       email:(NSString*)email
                      avatar:(UIImage*)avatar
                     managed:(BOOL)managed {
  self = [super init];
  if (self) {
    CHECK(avatar);
    CHECK(email);
    _avatar = avatar;
    _name = name;
    _email = email;
    _managed = managed;
    // Ensure the absence of the standard MDCSnacbarMessage’s text.
    self.text = @"";
    // Allows snackbar to stay longer in some tests.
    base::TimeDelta overridden_duration =
        tests_hook::GetOverriddenSnackbarDuration();
    if (overridden_duration.InSeconds() != 0) {
      self.duration = overridden_duration.InSeconds();
    }
    base::UmaHistogramBoolean(
        /*name=*/kIdentitySnackbarHadUserName,
        /*sample=*/(_name != nil));
  }
  return self;
}

- (Class)viewClass {
  return [IdentitySnackbarMessageView class];
}

#pragma mark - NSCopying

- (instancetype)copyWithZone:(NSZone*)zone {
  // The Snackbar Manager copy the sncakbar message.
  // So this need to be implemented.
  IdentitySnackbarMessage* instance = [super copyWithZone:zone];
  instance.avatar = _avatar;
  instance.name = _name;
  instance.email = _email;
  instance.managed = _managed;
  return instance;
}

@end
