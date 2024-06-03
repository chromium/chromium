// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/spotlight/searchable_item_factory.h"

#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#import <string>

#import "base/containers/span.h"
#import "base/functional/bind.h"
#import "base/hash/md5.h"
#import "base/memory/raw_ptr.h"
#import "base/numerics/byte_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/cancelable_task_tracker.h"
#import "build/branding_buildflags.h"
#import "components/favicon/core/fallback_url_util.h"
#import "components/favicon/core/large_icon_service.h"
#import "components/favicon_base/fallback_icon_style.h"
#import "components/favicon_base/favicon_types.h"
#import "ios/chrome/app/spotlight/spotlight_logger.h"
#import "ios/chrome/grit/ios_strings.h"
#import "net/base/apple/url_conversions.h"
#import "skia/ext/skia_utils_ios.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
// Minimum size of the icon to be used in Spotlight.
const NSInteger kMinIconSize = 32;

// Preferred size of the icon to be used in Spotlight.
const NSInteger kIconSize = 64;

// Size of the fallback icon.
const CGFloat kFallbackIconSize = 180;

// Radius of the rounded corner of the fallback icon.
const CGFloat kFallbackRoundedCorner = 8;

// Create an image with a rounded square with color `backgroundColor` and
// `string` centered in color `textColor`.
UIImage* GetFallbackImageWithStringAndColor(NSString* string,
                                            UIColor* backgroundColor,
                                            UIColor* textColor) {
  CGRect rect = CGRectMake(0, 0, kFallbackIconSize, kFallbackIconSize);
  UIGraphicsBeginImageContext(rect.size);
  CGContextRef context = UIGraphicsGetCurrentContext();
  CGContextSetFillColorWithColor(context, [backgroundColor CGColor]);
  CGContextSetStrokeColorWithColor(context, [textColor CGColor]);
  UIBezierPath* rounded =
      [UIBezierPath bezierPathWithRoundedRect:rect
                                 cornerRadius:kFallbackRoundedCorner];
  [rounded fill];
  UIFont* font = [UIFont systemFontOfSize:(kFallbackIconSize / 2)
                                   weight:UIFontWeightRegular];
  CGRect textRect = CGRectMake(0, (kFallbackIconSize - [font lineHeight]) / 2,
                               kFallbackIconSize, [font lineHeight]);
  NSMutableParagraphStyle* paragraphStyle =
      [[NSMutableParagraphStyle alloc] init];
  [paragraphStyle setAlignment:NSTextAlignmentCenter];
  NSMutableDictionary* attributes = [[NSMutableDictionary alloc] init];
  [attributes setValue:font forKey:NSFontAttributeName];
  [attributes setValue:textColor forKey:NSForegroundColorAttributeName];
  [attributes setValue:paragraphStyle forKey:NSParagraphStyleAttributeName];

  [string drawInRect:textRect withAttributes:attributes];
  UIImage* image = UIGraphicsGetImageFromCurrentImageContext();
  UIGraphicsEndImageContext();
  return image;
}

}  // namespace

@interface SearchableItemFactory () {
  // Domain identifier of the searchableItems managed by the factory.
  spotlight::Domain _spotlightDomain;

  // Service to retrieve large favicon or colors for a fallback icon.
  raw_ptr<favicon::LargeIconService> _largeIconService;  // weak

  // Queue to query large icons.
  std::unique_ptr<base::CancelableTaskTracker> _largeIconTaskTracker;
}

// Returns an array of Keywords for Spotlight search.
- (NSArray*)keywordsForSpotlightItems;

@end

@implementation SearchableItemFactory

- (instancetype)initWithLargeIconService:
                    (favicon::LargeIconService*)largeIconService
                                  domain:(spotlight::Domain)domain
                   useTitleInIdentifiers:(BOOL)useTitleInIdentifiers {
  self = [super init];
  if (self) {
    _largeIconService = largeIconService;
    _spotlightDomain = domain;
    _largeIconTaskTracker = std::make_unique<base::CancelableTaskTracker>();
    _useTitleInIdentifiers = useTitleInIdentifiers;
  }
  return self;
}

- (void)dealloc {
  if (_largeIconTaskTracker) {
    _largeIconTaskTracker->TryCancelAll();
    _largeIconTaskTracker.reset();
    _largeIconService = nullptr;
  }
}

- (void)generateSearchableItem:(const GURL&)URLToRefresh
                         title:(NSString*)title
            additionalKeywords:(NSArray<NSString*>*)keywords
             completionHandler:(void (^)(CSSearchableItem*))completionHandler {
  if (!URLToRefresh.is_valid() || ![title length]) {
    return;
  }

  __weak SearchableItemFactory* weakSelf = self;
  _largeIconService->GetLargeIconRawBitmapOrFallbackStyleForPageUrl(
      URLToRefresh, kMinIconSize * [UIScreen mainScreen].scale,
      kIconSize * [UIScreen mainScreen].scale,
      base::BindOnce(
          ^(const GURL& itemURL, const favicon_base::LargeIconResult& result) {
            [weakSelf largeIconResult:result
                              itemURL:itemURL
                                title:title
                   additionalKeywords:keywords
                    completionHandler:completionHandler];
          },
          URLToRefresh),
      _largeIconTaskTracker.get());
}

- (CSSearchableItem*)searchableItem:(NSString*)title
                             itemID:(NSString*)itemID
                 additionalKeywords:(NSArray<NSString*>*)keywords {
  CSSearchableItemAttributeSet* attributeSet =
      [[CSSearchableItemAttributeSet alloc]
          initWithItemContentType:spotlight::StringFromSpotlightDomain(
                                      _spotlightDomain)];
  [attributeSet setTitle:title];
  [attributeSet setDisplayName:title];

  CSSearchableItem* item = [self spotlightItemWithItemID:itemID
                                            attributeSet:attributeSet];

  [self addKeywords:keywords toSearchableItem:item];

  return item;
}

- (NSString*)spotlightIDForURL:(const GURL&)URL {
  return [self spotlightIDForURL:URL title:@""];
}

- (NSString*)spotlightIDForURL:(const GURL&)URL title:(NSString*)title {
  NSString* spotlightID = [NSString
      stringWithFormat:@"%@.%016llx",
                       spotlight::StringFromSpotlightDomain(_spotlightDomain),
                       [self hashForURL:URL title:title]];
  return spotlightID;
}

- (void)cancelItemsGeneration {
  _largeIconTaskTracker->TryCancelAll();
}

#pragma mark private methods

// Calls a completion handler after creating a searchable item with
// url,title,keywords and a favicon.
- (void)largeIconResult:(const favicon_base::LargeIconResult&)largeIconResult
                itemURL:(const GURL&)itemURL
                  title:(NSString*)title
     additionalKeywords:(NSArray<NSString*>*)keywords
      completionHandler:(void (^)(CSSearchableItem*))completionHandler {
  UIImage* favicon;

  if (largeIconResult.bitmap.is_valid()) {
    scoped_refptr<base::RefCountedMemory> data =
        largeIconResult.bitmap.bitmap_data;
    favicon = [UIImage imageWithData:[NSData dataWithBytes:data->front()
                                                    length:data->size()]
                               scale:[UIScreen mainScreen].scale];
  } else {
    NSString* iconText =
        base::SysUTF16ToNSString(favicon::GetFallbackIconText(itemURL));
    UIColor* backgroundColor = skia::UIColorFromSkColor(
        largeIconResult.fallback_icon_style->background_color);
    UIColor* textColor = skia::UIColorFromSkColor(
        largeIconResult.fallback_icon_style->text_color);
    favicon = GetFallbackImageWithStringAndColor(iconText, backgroundColor,
                                                 textColor);
  }

  CSSearchableItem* spotlightItem = [self spotlightItemWithURL:itemURL
                                                       favicon:favicon
                                                  defaultTitle:title];

  [self addKeywords:keywords toSearchableItem:spotlightItem];

  completionHandler(spotlightItem);
}

// Returns an array with default keywords for a spotlight item.
- (NSArray*)keywordsForSpotlightItems {
  return @[
    l10n_util::GetNSString(IDS_IOS_SPOTLIGHT_KEYWORD_ONE),
    l10n_util::GetNSString(IDS_IOS_SPOTLIGHT_KEYWORD_TWO),
    l10n_util::GetNSString(IDS_IOS_SPOTLIGHT_KEYWORD_THREE),
    l10n_util::GetNSString(IDS_IOS_SPOTLIGHT_KEYWORD_FOUR),
    l10n_util::GetNSString(IDS_IOS_SPOTLIGHT_KEYWORD_FIVE),
    l10n_util::GetNSString(IDS_IOS_SPOTLIGHT_KEYWORD_SIX),
    l10n_util::GetNSString(IDS_IOS_SPOTLIGHT_KEYWORD_SEVEN),
    l10n_util::GetNSString(IDS_IOS_SPOTLIGHT_KEYWORD_EIGHT),
    l10n_util::GetNSString(IDS_IOS_SPOTLIGHT_KEYWORD_NINE),
    l10n_util::GetNSString(IDS_IOS_SPOTLIGHT_KEYWORD_TEN),
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    @"google",
    @"chrome",
#else
    @"chromium",
#endif
  ];
}

// Creates a searchable item with URL, favicon and a title.
- (CSSearchableItem*)spotlightItemWithURL:(const GURL&)indexedURL
                                  favicon:(UIImage*)favicon
                             defaultTitle:(NSString*)defaultTitle {
  DCHECK(defaultTitle);
  NSURL* nsURL = net::NSURLWithGURL(indexedURL);
  std::string description = indexedURL.SchemeIsCryptographic()
                                ? indexedURL.DeprecatedGetOriginAsURL().spec()
                                : indexedURL.spec();

  CSSearchableItemAttributeSet* attributeSet =
      [[CSSearchableItemAttributeSet alloc] initWithContentType:UTTypeURL];
  [attributeSet setTitle:defaultTitle];
  [attributeSet setDisplayName:defaultTitle];
  [attributeSet setURL:nsURL];
  [attributeSet setContentURL:nsURL];
  [attributeSet setContentDescription:base::SysUTF8ToNSString(description)];
  [attributeSet setThumbnailData:UIImagePNGRepresentation(favicon)];

  NSString* itemID = self.useTitleInIdentifiers
                         ? [self spotlightIDForURL:indexedURL
                                             title:defaultTitle]
                         : [self spotlightIDForURL:indexedURL];
  return [self spotlightItemWithItemID:itemID attributeSet:attributeSet];
}

// Creates a searchable item with a given ID and an attributeSet.
- (CSSearchableItem*)spotlightItemWithItemID:(NSString*)itemID
                                attributeSet:(CSSearchableItemAttributeSet*)
                                                 attributeSet {
  CSCustomAttributeKey* key = [[CSCustomAttributeKey alloc]
          initWithKeyName:spotlight::GetSpotlightCustomAttributeItemID()
               searchable:YES
      searchableByDefault:YES
                   unique:YES
              multiValued:NO];
  [attributeSet setValue:itemID forCustomKey:key];
  attributeSet.keywords = [self keywordsForSpotlightItems];
  attributeSet.containerDisplayName =
      spotlight::SpotlightItemSourceLabelFromDomain(_spotlightDomain);

  NSString* domainID = spotlight::StringFromSpotlightDomain(_spotlightDomain);

  return [[CSSearchableItem alloc] initWithUniqueIdentifier:itemID
                                           domainIdentifier:domainID
                                               attributeSet:attributeSet];
}

// Adds additional keywords to a searchable item.
- (void)addKeywords:(NSArray<NSString*>*)keywords
    toSearchableItem:(CSSearchableItem*)item {
  NSSet* itemKeywords = [NSSet setWithArray:[[item attributeSet] keywords]];
  itemKeywords = [itemKeywords setByAddingObjectsFromArray:keywords];
  [[item attributeSet] setKeywords:[itemKeywords allObjects]];
}

// Compute a hash consisting of the first 8 bytes of the MD5 hash of a string
// containing `URL` and `title`.
- (int64_t)hashForURL:(const GURL&)URL title:(NSString*)title {
  NSString* key = [NSString
      stringWithFormat:@"%@ %@", base::SysUTF8ToNSString(URL.spec()), title];
  const std::string clipboard = base::SysNSStringToUTF8(key);

  base::MD5Digest hash;
  base::MD5Sum(base::as_byte_span(clipboard), &hash);
  return base::U64FromLittleEndian(base::span(hash.a).first<8u>());
}

@end
