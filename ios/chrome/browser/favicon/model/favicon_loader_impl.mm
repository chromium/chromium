// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/favicon/model/favicon_loader_impl.h"

#import <UIKit/UIKit.h>

#import <functional>
#import <string>

#import "base/apple/foundation_util.h"
#import "base/functional/bind.h"
#import "base/memory/weak_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "components/favicon/core/fallback_url_util.h"
#import "components/favicon/core/large_icon_service.h"
#import "components/favicon_base/fallback_icon_style.h"
#import "components/favicon_base/favicon_callback.h"
#import "components/favicon_base/favicon_types.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "net/traffic_annotation/network_traffic_annotation.h"
#import "skia/ext/skia_utils_ios.h"
#import "url/gurl.h"

namespace {

// NetworkTrafficAnnotationTag for fetching favicon from a Google server.
const net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("favicon_loader_get_large_icon", R"(
        semantics {
        sender: "FaviconLoader"
        description:
            "Sends a request to a Google server to retrieve the favicon bitmap."
        trigger:
            "A request can be sent if Chrome does not have a favicon."
        data: "Page URL and desired icon size."
        destination: GOOGLE_OWNED_SERVICE
        }
        policy {
        cookies_allowed: NO
        setting: "This feature cannot be disabled by settings."
        policy_exception_justification: "Not implemented."
        }
        )");

}  // namespace

// Class used as a key in the NSCache dictionary.
//
// Allow to cache data for a pair of an URL and the favicon size.
@interface FaviconLoaderCacheKey : NSObject

- (instancetype)initWithURL:(const GURL&)URL
               sizeInPoints:(float)sizeInPoints NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

@implementation FaviconLoaderCacheKey {
  std::string _URLSpec;
  int _sizeInPoints;
}

- (instancetype)initWithURL:(const GURL&)URL sizeInPoints:(float)sizeInPoints {
  if ((self = [super init])) {
    _URLSpec = URL.is_valid() ? URL.spec() : std::string();
    _sizeInPoints = static_cast<int>(round(sizeInPoints));
  }
  return self;
}

- (BOOL)isEqual:(NSObject*)object {
  FaviconLoaderCacheKey* other =
      base::apple::ObjCCast<FaviconLoaderCacheKey>(object);
  return [self isEqualToCacheKey:other];
}

- (NSUInteger)hash {
  const size_t h1 = std::hash<std::string>{}(_URLSpec);
  const size_t h2 = std::hash<int>{}(_sizeInPoints);
  return h1 ^ (h2 << 1);
}

- (BOOL)isEqualToCacheKey:(FaviconLoaderCacheKey*)other {
  if (!other) {
    return NO;
  }

  if (self == other) {
    return YES;
  }

  return _sizeInPoints == other->_sizeInPoints && _URLSpec == other->_URLSpec;
}

@end

// Class representing the parameters for fetching a favicon.
class FaviconLoaderImpl::Request {
 public:
  using Completion = FaviconLoader::FaviconAttributesCompletionBlock;

  // The different types of requests.
  enum class Type {
    kFaviconForPageUrl,
    kFaviconForPageUrlWithFallback,
    kFaviconForPageUrlOrHost,
    kFaviconForIconUrl,
  };

  Request(Request&&) = default;

  ~Request() = default;

  // Constructs an identical Request without fallback to google servers.
  Request WithoutFallback() {
    DCHECK_EQ(type_, Type::kFaviconForPageUrlWithFallback);
    return Request{Type::kFaviconForPageUrl, url_, size_in_points_,
                   min_size_in_points_, completion_};
  }

  // Constructs a Request to fetch favicon for a page URL, optionally with
  // fallback to google servers.
  static Request ForPageUrl(const GURL& page_url,
                            float size_in_points,
                            float min_size_in_points,
                            bool fallback_to_google_server,
                            Completion completion) {
    const Type type = fallback_to_google_server
                          ? Type::kFaviconForPageUrlWithFallback
                          : Type::kFaviconForPageUrl;
    return Request{type, page_url, size_in_points, min_size_in_points,
                   completion};
  }

  // Constructs a Request to fetch favicon for a page URL or just an host.
  static Request ForPageUrlOrHost(const GURL& page_url,
                                  float size_in_points,
                                  Completion completion) {
    return Request{Type::kFaviconForPageUrlOrHost, page_url, size_in_points,
                   /*min_size_in_points=*/0.0, completion};
  }

  // Constructs a Request to fetch favicon from an icon url directly.
  static Request ForIconUrl(const GURL& icon_url,
                            float size_in_points,
                            float min_size_in_points,
                            Completion completion) {
    return Request{Type::kFaviconForIconUrl, icon_url, size_in_points,
                   min_size_in_points, completion};
  }

  // Return the request's type.
  Type type() const { return type_; }

  // Returns the requet's URL.
  const GURL& url() const { return url_; }

  // Returns the size in points for the favicon.
  float size_in_points() const { return size_in_points_; }

  // Returns the minimum size in points for the favicon.
  float min_size_in_points() const { return min_size_in_points_; }

  // Returns the key to use when locating the favicon in the cache.
  FaviconLoaderCacheKey* key() const {
    return [[FaviconLoaderCacheKey alloc]
         initWithURL:url_
        sizeInPoints:static_cast<int>(round(size_in_points_))];
  }

  // Invokes the request's completion block with `attributes`.
  void completion(FaviconAttributes* attributes, bool cached) {
    completion_(attributes, cached);
  }

 private:
  Request(Type type,
          const GURL& url,
          float size_in_points,
          float min_size_in_points,
          Completion completion)
      : type_(type),
        url_(url),
        size_in_points_(size_in_points),
        min_size_in_points_(min_size_in_points),
        completion_(completion) {
    DCHECK(completion_);
  }

  const Type type_;
  const GURL url_;
  const float size_in_points_;
  const float min_size_in_points_;
  const Completion completion_;
};

FaviconLoaderImpl::FaviconLoaderImpl(
    favicon::LargeIconService* large_icon_service)
    : large_icon_service_(large_icon_service),
      favicon_cache_([[NSCache alloc] init]) {}

FaviconLoaderImpl::~FaviconLoaderImpl() = default;

void FaviconLoaderImpl::FaviconForPageUrl(
    const GURL& page_url,
    float size_in_points,
    float min_size_in_points,
    bool fallback_to_google_server,
    FaviconAttributesCompletionBlock completion) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(completion);
  FetchFavicon(Request::ForPageUrl(page_url, size_in_points, min_size_in_points,
                                   fallback_to_google_server, completion));
}

void FaviconLoaderImpl::FaviconForPageUrlOrHost(
    const GURL& page_url,
    float size_in_points,
    FaviconAttributesCompletionBlock completion) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(completion);
  FetchFavicon(Request::ForPageUrlOrHost(page_url, size_in_points, completion));
}

void FaviconLoaderImpl::FaviconForIconUrl(
    const GURL& icon_url,
    float size_in_points,
    float min_size_in_points,
    FaviconAttributesCompletionBlock completion) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(completion);
  FetchFavicon(Request::ForIconUrl(icon_url, size_in_points, min_size_in_points,
                                   completion));
}

void FaviconLoaderImpl::CancellAllRequests() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cancelable_task_tracker_.TryCancelAll();
}

base::WeakPtr<FaviconLoader> FaviconLoaderImpl::AsWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return weak_ptr_factory_.GetWeakPtr();
}

void FaviconLoaderImpl::FetchFavicon(Request request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  FaviconAttributes* cached_value = GetCachedAttributes(request.key());

  // First check whether the favicon is present in the cache. If the cache
  // doesn't contain an image, it is only worth continuing if there is a
  // fallback mechanism.
  if (cached_value &&
      (cached_value.faviconImage ||
       request.type() != Request::Type::kFaviconForPageUrlWithFallback)) {
    request.completion(cached_value, /*cached*/ true);
    return;
  }

  FaviconAttributes* attributes = [FaviconAttributes
      attributesWithMonogram:base::SysUTF16ToNSString(
                                 favicon::GetFallbackIconText(request.url()))
                   textColor:[UIColor colorWithWhite:
                                          kFallbackIconDefaultTextColorGrayscale
                                               alpha:1]
             backgroundColor:UIColor.clearColor
      defaultBackgroundColor:YES];

  // If the favicon was not cached, then return a fallback image synchronously.
  request.completion(attributes, /*cached*/ true);

  // Fetch asynchronously a better favicon using the LargeIconServvice.
  DCHECK(large_icon_service_);
  const GURL url = request.url();
  const CGFloat scale = UIScreen.mainScreen.scale;
  const CGFloat size_in_pixels = scale * request.size_in_points();
  const CGFloat min_size_in_pixels = scale * request.min_size_in_points();

  auto callback =
      base::BindOnce(&FaviconLoaderImpl::OnFaviconFetched,
                     weak_ptr_factory_.GetWeakPtr(), scale, std::move(request));

  switch (request.type()) {
    case Request::Type::kFaviconForPageUrl:
    case Request::Type::kFaviconForPageUrlWithFallback:
      large_icon_service_->GetLargeIconRawBitmapOrFallbackStyleForPageUrl(
          url, min_size_in_pixels, size_in_pixels, std::move(callback),
          &cancelable_task_tracker_);
      break;

    case Request::Type::kFaviconForPageUrlOrHost:
      large_icon_service_->GetIconRawBitmapOrFallbackStyleForPageUrl(
          url, size_in_pixels, std::move(callback), &cancelable_task_tracker_);
      break;

    case Request::Type::kFaviconForIconUrl:
      large_icon_service_->GetLargeIconRawBitmapOrFallbackStyleForIconUrl(
          url, min_size_in_pixels, size_in_pixels, std::move(callback),
          &cancelable_task_tracker_);
      break;
  }
}

void FaviconLoaderImpl::OnFaviconFetched(
    CGFloat scale,
    Request request,
    const favicon_base::LargeIconResult& result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // LargeIconResult is either a valid favicon (which can be the default
  // favicon) or fallback attributes.
  if (result.bitmap.is_valid()) {
    scoped_refptr<base::RefCountedMemory> data =
        result.bitmap.bitmap_data.get();

    // The favicon code assumes favicons are PNG-encoded.
    UIImage* favicon = [UIImage
        imageWithData:[NSData dataWithBytes:data->front() length:data->size()]
                scale:scale];
    FaviconAttributes* attributes =
        [FaviconAttributes attributesWithImage:favicon];
    StoreAttributesInCache(attributes, request.key());

    DCHECK(favicon.size.width <= request.size_in_points() &&
           favicon.size.height <= request.size_in_points());
    request.completion(attributes, /*cached*/ false);
    return;
  }

  if (request.type() == Request::Type::kFaviconForPageUrlWithFallback) {
    const GURL url = request.url();
    large_icon_service_
        ->GetLargeIconOrFallbackStyleFromGoogleServerSkippingLocalCache(
            url,
            /*should_trim_page_url_path=*/false, kTrafficAnnotation,
            base::BindOnce(&FaviconLoaderImpl::OnGoogleServerFallbackCompleted,
                           weak_ptr_factory_.GetWeakPtr(), std::move(request)));
    return;
  }

  // Did not get valid favicon back and are not attempting to retrieve one
  // from a Google Server.
  DCHECK(result.fallback_icon_style);
  FaviconAttributes* attributes = [FaviconAttributes
      attributesWithMonogram:base::SysUTF16ToNSString(
                                 favicon::GetFallbackIconText(request.url()))
                   textColor:[UIColor colorWithWhite:
                                          kFallbackIconDefaultTextColorGrayscale
                                               alpha:1]
             backgroundColor:UIColor.clearColor
      defaultBackgroundColor:YES];
  request.completion(attributes, /*cached*/ false);
}

void FaviconLoaderImpl::OnGoogleServerFallbackCompleted(
    Request request,
    favicon_base::GoogleFaviconServerRequestStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Update the time when the icon was last requested - postpone thus
  // the automatic eviction of the favicon from the favicon database.
  large_icon_service_->TouchIconFromGoogleServer(request.url());

  // Favicon should be loaded to the db that backs LargeIconService
  // now.  Fetch it again. Even if the request was not successful, the
  // fallback style will be used.
  FetchFavicon(request.WithoutFallback());
}

FaviconAttributes* FaviconLoaderImpl::GetCachedAttributes(
    FaviconLoaderCacheKey* key) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return [favicon_cache_ objectForKey:key];
}

void FaviconLoaderImpl::StoreAttributesInCache(FaviconAttributes* attributes,
                                               FaviconLoaderCacheKey* key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  [favicon_cache_ setObject:attributes forKey:key];
}
