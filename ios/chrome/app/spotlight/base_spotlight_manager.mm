// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/spotlight/base_spotlight_manager.h"

#import <MobileCoreServices/MobileCoreServices.h>

#import <memory>
#import <set>
#import <string>

#import "base/bind.h"
#import "base/containers/contains.h"
#import "base/hash/md5.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/cancelable_task_tracker.h"
#import "build/branding_buildflags.h"
#import "components/favicon/core/fallback_url_util.h"
#import "components/favicon/core/large_icon_service.h"
#import "components/favicon_base/fallback_icon_style.h"
#import "components/favicon_base/favicon_types.h"
#import "ios/chrome/grit/ios_strings.h"
#import "net/base/mac/url_conversions.h"
#import "skia/ext/skia_utils_ios.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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

@interface BaseSpotlightManager () {
  // Domain of the spotlight manager.
  spotlight::Domain _spotlightDomain;

  // Service to retrieve large favicon or colors for a fallback icon.
  favicon::LargeIconService* _largeIconService;  // weak

  // Queue to query large icons.
  std::unique_ptr<base::CancelableTaskTracker> _largeIconTaskTracker;

  // Tasks querying the large icons.
  std::set<GURL> _pendingTasks;

  // Records whether -shutdown has been invoked and the method forwarded to
  // the base class.
  BOOL _shutdownCalled;
}

// Compute a hash consisting of the first 8 bytes of the MD5 hash of a string
// containing `URL` and `title`.
- (int64_t)hashForURL:(const GURL&)URL title:(NSString*)title;

// Returns an array of Keywords for Spotlight search.
- (NSArray*)keywordsForSpotlightItems;

@end

@implementation BaseSpotlightManager

- (instancetype)initWithLargeIconService:
                    (favicon::LargeIconService*)largeIconService
                                  domain:(spotlight::Domain)domain {
  self = [super init];
  if (self) {
    _spotlightDomain = domain;
    _largeIconService = largeIconService;
    _largeIconTaskTracker = std::make_unique<base::CancelableTaskTracker>();
  }
  return self;
}

- (void)dealloc {
  DCHECK(_shutdownCalled);
}

- (int64_t)hashForURL:(const GURL&)URL title:(NSString*)title {
  NSString* key = [NSString
      stringWithFormat:@"%@ %@", base::SysUTF8ToNSString(URL.spec()), title];
  const std::string clipboard = base::SysNSStringToUTF8(key);
  const char* c_string = clipboard.c_str();

  base::MD5Digest hash;
  base::MD5Sum(c_string, strlen(c_string), &hash);
  uint64_t md5 = *(reinterpret_cast<uint64_t*>(hash.a));
  return md5;
}

- (NSString*)spotlightIDForURL:(const GURL&)URL title:(NSString*)title {
  NSString* spotlightID = [NSString
      stringWithFormat:@"%@.%016llx",
                       spotlight::StringFromSpotlightDomain(_spotlightDomain),
                       [self hashForURL:URL title:title]];
  return spotlightID;
}

- (void)cancelAllLargeIconPendingTasks {
  DCHECK(!_shutdownCalled);
  _largeIconTaskTracker->TryCancelAll();
  _pendingTasks.clear();
}

- (void)clearAllSpotlightItems:(BlockWithError)callback {
  [self cancelAllLargeIconPendingTasks];
  spotlight::DeleteSearchableDomainItems(_spotlightDomain, callback);
}

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

  NSString* domainID = spotlight::StringFromSpotlightDomain(_spotlightDomain);

  return [[CSSearchableItem alloc] initWithUniqueIdentifier:itemID
                                           domainIdentifier:domainID
                                               attributeSet:attributeSet];
}

- (NSArray*)spotlightItemsWithURL:(const GURL&)indexedURL
                          favicon:(UIImage*)favicon
                     defaultTitle:(NSString*)defaultTitle {
  DCHECK(defaultTitle);
  NSURL* nsURL = net::NSURLWithGURL(indexedURL);
  std::string description = indexedURL.SchemeIsCryptographic()
                                ? indexedURL.DeprecatedGetOriginAsURL().spec()
                                : indexedURL.spec();

  CSSearchableItemAttributeSet* attributeSet =
      [[CSSearchableItemAttributeSet alloc]
          initWithItemContentType:(NSString*)kUTTypeURL];
  [attributeSet setTitle:defaultTitle];
  [attributeSet setDisplayName:defaultTitle];
  [attributeSet setURL:nsURL];
  [attributeSet setContentURL:nsURL];
  [attributeSet setContentDescription:base::SysUTF8ToNSString(description)];
  [attributeSet setThumbnailData:UIImagePNGRepresentation(favicon)];

  NSString* itemID = [self spotlightIDForURL:indexedURL title:defaultTitle];
  return [NSArray arrayWithObject:[self spotlightItemWithItemID:itemID
                                                   attributeSet:attributeSet]];
}

- (void)refreshItemsWithURL:(const GURL&)URLToRefresh title:(NSString*)title {
  DCHECK(!_shutdownCalled);
  if (!URLToRefresh.is_valid() || base::Contains(_pendingTasks, URLToRefresh))
    return;

  _pendingTasks.insert(URLToRefresh);
  __weak BaseSpotlightManager* weakSelf = self;
  base::CancelableTaskTracker::TaskId taskID =
      _largeIconService->GetLargeIconRawBitmapOrFallbackStyleForPageUrl(
          URLToRefresh, kMinIconSize * [UIScreen mainScreen].scale,
          kIconSize * [UIScreen mainScreen].scale,
          base::BindOnce(
              ^(const GURL& itemURL,
                const favicon_base::LargeIconResult& result) {
                [weakSelf largeIconResult:result itemURL:itemURL title:title];
              },
              URLToRefresh),
          _largeIconTaskTracker.get());

  if (taskID == base::CancelableTaskTracker::kBadTaskId)
    _pendingTasks.erase(URLToRefresh);
}

- (NSUInteger)pendingLargeIconTasksCount {
  return static_cast<NSUInteger>(_pendingTasks.size());
}

- (void)shutdown {
  [self cancelAllLargeIconPendingTasks];
  _largeIconTaskTracker.reset();
  _largeIconService = nullptr;
  _shutdownCalled = YES;
}

#pragma mark private methods

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

- (void)largeIconResult:(const favicon_base::LargeIconResult&)largeIconResult
                itemURL:(const GURL&)itemURL
                  title:(NSString*)title {
  _pendingTasks.erase(itemURL);

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
  NSArray* spotlightItems = [self spotlightItemsWithURL:itemURL
                                                favicon:favicon
                                           defaultTitle:title];

  if ([spotlightItems count]) {
    [[CSSearchableIndex defaultSearchableIndex]
        indexSearchableItems:spotlightItems
           completionHandler:nil];
  }
}

@end
