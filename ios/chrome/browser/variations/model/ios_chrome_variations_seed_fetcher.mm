// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/variations/model/ios_chrome_variations_seed_fetcher.h"
#import "ios/chrome/browser/variations/model/ios_chrome_variations_seed_fetcher+testing.h"

#import "base/metrics/histogram_functions.h"
#import "base/notreached.h"
#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "build/branding_buildflags.h"
#import "components/variations/seed_response.h"
#import "components/variations/variations_switches.h"
#import "components/variations/variations_url_constants.h"
#import "components/version_info/version_info.h"
#import "ios/chrome/browser/variations/model/constants.h"
#import "ios/chrome/browser/variations/model/ios_chrome_variations_seed_store.h"
#import "ios/chrome/common/channel_info.h"
#import "net/http/http_status_code.h"

#import "ios/chrome/browser/variations/model/ios_chrome_variations_seed_store+fetcher.h"

namespace {

// Maximum time allowed to fetch the seed before the request is cancelled.
const base::TimeDelta kRequestTimeout = base::Seconds(1.5);
// Histogram names for seed fetch time and result.
const char kSeedFetchResultHistogram[] =
    "IOS.Variations.FirstRun.SeedFetchResult";
const char kSeedFetchTimeHistogram[] = "IOS.Variations.FirstRun.SeedFetchTime";

// Whether a current request for variations seed is being made.
//
// This variable exists so that only one instance of the manager updates the
// global seed at one time. It is access in the static serial queue
// "*.first_run_variations_seed_manager" at the start of each task in the queue.
// If the value is NO, it's set to YES and keep executing the task; otherwise,
// it aborts the task to make sure the fetch result won't be overriden.
static BOOL g_seed_fetching_in_progress = NO;

}  // namespace

@implementation IOSChromeVariationsSeedFetcher {
  // Whether the current binary should fetch Finch seed for experiment purpose.
  // Accessed on the main thread.
  BOOL _fetchingEnabled;

  // The variations server domain name. Accessed on the main thread.
  std::string _variationsDomain;

  // The forced channel string retrieved from the command line. Accessed on the
  // main thread.
  std::string _forcedChannel;

  // The timestamp when the current seed request starts. This is used for metric
  // reporting, and will be reset to null value when the request finishes.
  // Accessed in the static serial queue "*.first_run_variations_seed_manager".
  base::Time _startTimeOfOngoingSeedRequest;
}

#pragma mark - Public

- (instancetype)initWithArguments:(NSArray<NSString*>*)arguments {
  self = [super init];
  if (self) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    _fetchingEnabled = YES;
#else
    _fetchingEnabled = NO;
#endif
    _variationsDomain = variations::kDefaultServerUrl;
    _forcedChannel = std::string();

    std::string url_switch =
        "--" + std::string(variations::switches::kVariationsServerURL) + "=";
    std::string channel_switch =
        "--" + std::string(variations::switches::kFakeVariationsChannel) + "=";
    for (NSString* a in arguments) {
      std::string arg = base::SysNSStringToUTF8(a);

      if (base::StartsWith(arg, url_switch)) {
        _variationsDomain = arg.substr(url_switch.size());
        if (!_fetchingEnabled && !_variationsDomain.empty()) {
          _fetchingEnabled = YES;
        }
      } else if (base::StartsWith(arg, channel_switch)) {
        _forcedChannel = arg.substr(channel_switch.size());
      }
    }
  }
  return self;
}

- (instancetype)init {
  return [self initWithArguments:[[NSProcessInfo processInfo] arguments]];
}

- (void)startSeedFetch {
  if (!_fetchingEnabled) {
    // Stops executing if seed fetching is disabled.
    [self notifyDelegateSeedFetchResult:NO];
    return;
  }
  // Set up a serial queue to to avoid concurrent read/write to static data.
  static dispatch_once_t onceToken;
  static dispatch_queue_t queue = nil;
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  const char* label = "com.google.chrome.first_run_variations_seed_manager";
#else
  const char* label = "org.chromium.first_run_variations_seed_manager";
#endif
  dispatch_once(&onceToken, ^{
    queue = dispatch_queue_create(label, DISPATCH_QUEUE_SERIAL);
  });

  // Adds the task of fetching the seed to the static serial queue, and return
  // from `doActualFetch` immediately. Note that the block will retain `self`.
  dispatch_async(queue, ^{
    if (g_seed_fetching_in_progress) {
      NOTREACHED_IN_MIGRATION()
          << "SeedFetch started while already in progress";
      [self notifyDelegateSeedFetchResult:NO];
    } else {
      [self doActualFetch];
    }
  });
}

#pragma mark - Private

// The URL of the variations server, including query parameters that identifies
// the request initiator. Accessed in the static serial queue
// "*.first_run_variations_seed_manager".
- (NSURL*)variationsURL {
  // Setting "osname", "milestone" and "channel" as parameters. Dogfood
  // experimenting is not supported on Chrome iOS, therefore we do not need the
  // "restrict" parameter.
  std::string queryString =
      "?osname=ios&milestone=" + version_info::GetMajorVersionNumber();
  std::string channel = _forcedChannel;
  if (channel.empty() && GetChannel() != version_info::Channel::UNKNOWN) {
    channel = GetChannelString();
  }
  if (!channel.empty()) {
    queryString += "&channel=" + channel;
  }
  return [NSURL
      URLWithString:base::SysUTF8ToNSString(_variationsDomain + queryString)];
}

// Helper method for `startSeedFetch` that initiates an HTTPS request to the
// Finch server in the static serial queue
// "*.first_run_variations_seed_manager".
//
// Note that if this method is invoked, the seed would be fetched regardless of
// `_fetchingEnabled`, so please use with caution.
- (void)doActualFetch {
  g_seed_fetching_in_progress = YES;
  NSMutableURLRequest* request = [NSMutableURLRequest
       requestWithURL:[self variationsURL]
          cachePolicy:NSURLRequestReloadIgnoringLocalCacheData
      timeoutInterval:kRequestTimeout.InSecondsF()];
  // Pass only "gzip" as an accepted format. Do not pass delta compression
  // ("x-bm"), as it is not applicable on first run (since there is no
  // existing seed).
  [request setValue:@"gzip" forHTTPHeaderField:@"A-IM"];
  NSURLSessionDataTask* task = [[NSURLSession sharedSession]
      dataTaskWithRequest:request
        completionHandler:^(NSData* data, NSURLResponse* response,
                            NSError* error) {
          [self seedRequestDidCompleteWithData:data
                                      response:(NSHTTPURLResponse*)response
                                         error:error];
        }];
  _startTimeOfOngoingSeedRequest = base::Time::Now();
  [task resume];
}

// Completion handler of a URL session task. It generates the seed using the
// HTTPS response sent back from the Finch server, stores them in the shared
// seed, and records relevant metrics.
- (void)seedRequestDidCompleteWithData:(NSData*)data
                              response:(NSHTTPURLResponse*)httpResponse
                                 error:(NSError*)error {
  // Normally net::HTTP_NOT_MODIFIED should be considered as a
  // successful response, but it is not expected when the request does
  // not contain "If-None-Match" header.
  BOOL success = error == nil && httpResponse.statusCode == net::HTTP_OK;
  IOSSeedFetchException exception = IOSSeedFetchException::kNotApplicable;
  if (success) {
    DCHECK(!_startTimeOfOngoingSeedRequest.is_null());
    base::UmaHistogramTimes(kSeedFetchTimeHistogram,
                            base::Time::Now() - _startTimeOfOngoingSeedRequest);
    std::unique_ptr<variations::SeedResponse> seed =
        [self seedResponseForHTTPResponse:httpResponse data:data];
    if (seed) {
      [IOSChromeVariationsSeedStore updateSharedSeed:std::move(seed)];
    } else {
      // Currently, only the IM header is mandatory to create a first run seed,
      // and is the only possible reason that a seed is downloaded but not
      // created.
      exception = IOSSeedFetchException::kInvalidIMHeader;
      success = NO;
    }
  } else if (error.code == NSURLErrorTimedOut) {
    exception = IOSSeedFetchException::kHTTPSRequestTimeout;
  } else if (error.code == NSURLErrorBadURL ||
             error.code == NSURLErrorDNSLookupFailed ||
             error.code == NSURLErrorCannotFindHost) {
    exception = IOSSeedFetchException::kHTTPSRequestBadUrl;
  }
  _startTimeOfOngoingSeedRequest = base::Time();
  g_seed_fetching_in_progress = NO;

  // Log seed fetch result on UMA and notify delegate.
  int seedFetchResultValue = exception == IOSSeedFetchException::kNotApplicable
                                 ? static_cast<int>(httpResponse.statusCode)
                                 : static_cast<int>(exception);
  base::UmaHistogramSparse(kSeedFetchResultHistogram, seedFetchResultValue);
  [self notifyDelegateSeedFetchResult:success];
}

// Generates and returns the SeedResponse by parsing the HTTP response returned
// by the variations server. Returns `nil` if the HTTP response is invalid.
// Invoked in the completion handler of a URL session task.
- (std::unique_ptr<variations::SeedResponse>)
    seedResponseForHTTPResponse:(NSHTTPURLResponse*)httpResponse
                           data:(NSData*)data {
  NSString* signature =
      [httpResponse valueForHTTPHeaderField:@"X-Seed-Signature"];
  NSString* country = [httpResponse valueForHTTPHeaderField:@"X-Country"];

  // Returned seed should have been gzip compressed.
  NSCharacterSet* whitespace = [NSCharacterSet whitespaceCharacterSet];
  NSPredicate* nonEmpty = [NSPredicate
      predicateWithBlock:^BOOL(NSString* im, NSDictionary* bindings) {
        return [[im stringByTrimmingCharactersInSet:whitespace] length] > 0;
      }];
  NSArray<NSString*>* instanceManipulations = [[[httpResponse
      valueForHTTPHeaderField:@"IM"] componentsSeparatedByString:@","]
      filteredArrayUsingPredicate:nonEmpty];
  // Only gzip compressed data is supported on first run seed fetching with
  // "gzip" specified in the request.
  if ([instanceManipulations count] == 1 &&
      [[instanceManipulations[0] stringByTrimmingCharactersInSet:whitespace]
          isEqualToString:@"gzip"]) {
    auto seed = std::make_unique<variations::SeedResponse>();
    if (data) {
      // "data" is binary, for which protobuf uses strings.
      seed->data = std::string(reinterpret_cast<const char*>([data bytes]),
                               [data length]);
    }
    seed->signature = base::SysNSStringToUTF8(signature);
    seed->country = base::SysNSStringToUTF8(country);
    seed->date = base::Time::Now();
    seed->is_gzip_compressed = YES;
    return seed;
  }
  return nullptr;
}

// Notifies the delegate of the seed fetching result. Since the seed fetch
// request is sent on the background instead of the main queue, this method
// should explicitly dispatch the result back on the main queue.
- (void)notifyDelegateSeedFetchResult:(BOOL)result {
  __weak IOSChromeVariationsSeedFetcher* weakSelf = self;
  dispatch_async(dispatch_get_main_queue(), ^{
    [weakSelf.delegate variationsSeedFetcherDidCompleteFetchWithSuccess:result];
  });
}

// Invoked by the testing code to reset the fetching status after each test. DO
// NOT INVOKE IN PRODUCTION CODE.
+ (void)resetFetchingStatusForTesting {
  g_seed_fetching_in_progress = NO;
}

@end
