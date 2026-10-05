// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_item.h"

#import "ios/chrome/browser/composebox/shared/ui/composebox_ui_constants.h"

@implementation ComposeboxMenuItem

- (instancetype)initWithTitle:(NSString*)title
                     subtitle:(NSString*)subtitle
                     favicons:(NSArray<UIImage*>*)favicons
                        image:(UIImage*)image
                         type:(ComposeboxMenuItemType)type
                     disabled:(BOOL)disabled
                      favicon:(UIImage*)favicon {
  self = [super init];
  if (self) {
    _title = [title copy];
    _subtitle = [subtitle copy];
    _favicons = [favicons copy];
    _image = image;
    _type = type;
    _disabled = disabled;
    _favicon = favicon;
  }
  return self;
}

- (instancetype)initWithTitle:(NSString*)title
                     subtitle:(NSString*)subtitle
                     favicons:(NSArray<UIImage*>*)favicons
                         type:(ComposeboxMenuItemType)type {
  return [self initWithTitle:title
                    subtitle:subtitle
                    favicons:favicons
                       image:nil
                        type:type
                    disabled:NO
                     favicon:nil];
}

- (instancetype)initWithTitle:(NSString*)title
                        image:(UIImage*)image
                         type:(ComposeboxMenuItemType)type
                     disabled:(BOOL)disabled
                      favicon:(UIImage*)favicon {
  return [self initWithTitle:title
                    subtitle:nil
                    favicons:nil
                       image:image
                        type:type
                    disabled:disabled
                     favicon:favicon];
}

- (instancetype)initWithTitle:(NSString*)title
                        image:(UIImage*)image
                         type:(ComposeboxMenuItemType)type
                     disabled:(BOOL)disabled {
  return [self initWithTitle:title
                       image:image
                        type:type
                    disabled:disabled
                     favicon:nil];
}

- (instancetype)initWithTitle:(NSString*)title
                        image:(UIImage*)image
                         type:(ComposeboxMenuItemType)type {
  return [self initWithTitle:title
                       image:image
                        type:type
                    disabled:NO
                     favicon:nil];
}

- (BOOL)isAttachmentType {
  return self.type == ComposeboxMenuItemType::kCurrentTab ||
         self.type == ComposeboxMenuItemType::kAttachmentTabs ||
         self.type == ComposeboxMenuItemType::kAttachmentCamera ||
         self.type == ComposeboxMenuItemType::kAttachmentGallery ||
         self.type == ComposeboxMenuItemType::kAttachmentFiles ||
         self.type == ComposeboxMenuItemType::kAttachmentDrive;
}

- (BOOL)isEqual:(id)object {
  if (self == object) {
    return YES;
  }
  if (![object isKindOfClass:[ComposeboxMenuItem class]]) {
    return NO;
  }
  ComposeboxMenuItem* other = (ComposeboxMenuItem*)object;

  return self.type == other.type &&
         (self.subtitle == other.subtitle ||
          [self.subtitle isEqualToString:other.subtitle]) &&
         [self.title isEqualToString:other.title] &&
         (self.favicons == other.favicons ||
          [self.favicons isEqualToArray:other.favicons]) &&
         self.disabled == other.disabled &&
         (self.image == other.image || [self.image isEqual:other.image]) &&
         (self.favicon == other.favicon ||
          [self.favicon isEqual:other.favicon]);
}

- (NSUInteger)hash {
  return static_cast<NSUInteger>(self.type) ^ self.title.hash ^
         self.subtitle.hash ^ self.favicons.hash ^ self.disabled;
}

- (id)copyWithZone:(NSZone*)zone {
  return [[ComposeboxMenuItem alloc] initWithTitle:self.title
                                          subtitle:self.subtitle
                                          favicons:self.favicons
                                             image:self.image
                                              type:self.type
                                          disabled:self.disabled
                                           favicon:self.favicon];
}

@end

NSString* AccessibilityIdentifierForMenuItemType(ComposeboxMenuItemType type) {
  switch (type) {
    case ComposeboxMenuItemType::kAIM:
      return kComposeboxAIMActionAccessibilityIdentifier;
    case ComposeboxMenuItemType::kCreateImage:
      return kComposeboxImageGenerationActionAccessibilityIdentifier;
    case ComposeboxMenuItemType::kCurrentTab:
      return kComposeboxAttachCurrentTabActionAccessibilityIdentifier;
    case ComposeboxMenuItemType::kAttachmentTabs:
      return kComposeboxSelectTabsActionAccessibilityIdentifier;
    case ComposeboxMenuItemType::kAttachmentCamera:
      return kComposeboxCameraActionAccessibilityIdentifier;
    case ComposeboxMenuItemType::kAttachmentGallery:
      return kComposeboxGalleryActionAccessibilityIdentifier;
    case ComposeboxMenuItemType::kAttachmentFiles:
      return kComposeboxAttachFileActionAccessibilityIdentifier;
    case ComposeboxMenuItemType::kAttachmentDrive:
      return kComposeboxAttachDriveActionAccessibilityIdentifier;
    default:
      return nil;
  }
}
