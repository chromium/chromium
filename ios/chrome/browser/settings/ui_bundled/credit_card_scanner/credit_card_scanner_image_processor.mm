// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/credit_card_scanner/credit_card_scanner_image_processor.h"

#import <CoreMedia/CoreMedia.h>
#import <Vision/Vision.h>

#import "base/task/sequenced_task_runner.h"
#import "base/task/task_runner.h"
#import "ios/chrome/browser/settings/ui_bundled/credit_card_scanner/credit_card_scanner_consumer.h"
#import "ios/chrome/browser/settings/ui_bundled/credit_card_scanner/credit_card_scanner_string_util.h"

namespace {
// The minimum number of times a card number must be detected before we consider
// it a confident match.
//
// TODO(crbug.com/442869727): Determine the right trade-off for this number,
// between more times causing higher latency, versus fewer times causing a
// higher chance of a misrecognition.
const int kMinConfidenceCount = 15;

// Structure for storing possible candidates (for either a card number or
// expiration dates) that have been recognized, mapped to how many times (i.e.,
// in how many frames) a given candidate has been detected. As an optimization,
// tracks the current most-common key and its associated count.
//
// Adding to this type must be done by calling `setOrIncrementCount`.
struct CandidateDictionary {
  // The underlying dictionary that tracks the candidates and their associated
  // counts.
  NSMutableDictionary<id<NSCopying>, NSNumber*>* _counts =
      [NSMutableDictionary dictionary];

  // The currently tracked most common key. Of the same type as the keys in
  // `_counts`.
  id<NSCopying> _maxKey = nil;

  // The currently tracked count for `_maxKey`.
  NSNumber* _maxCount = 0;
};

// Helper struct for returning both a key and a count from `mostCommonKey`.
struct KeyAndCount {
  id<NSCopying> _key;
  int _count;
};
}  // namespace

@implementation CreditCardScannerImageProcessor {
  // An image analysis request that finds and recognizes text in an image.
  VNRecognizeTextRequest* _textRecognitionRequest;

  // The consumer to notify once a card number (and potentially expiry date)
  // have been extracted.
  __weak id<CreditCardScannerConsumer> _creditCardScannerConsumer;

  // The possible card numbers that have been recognized so far.
  CandidateDictionary _candidateCardNumbers;

  // The possible expiration dates that have been recognized so far.
  CandidateDictionary _candidateExpirationDates;

  // TaskRunner for the main thread, used to post results to the
  // `_creditCardScannerConsumer` from the vision API thread.
  scoped_refptr<base::SequencedTaskRunner> _mainThreadTaskRunner;
}

#pragma mark - Lifecycle

- (instancetype)initWithConsumer:(id<CreditCardScannerConsumer>)consumer {
  self = [super init];
  if (self) {
    _creditCardScannerConsumer = consumer;
    _mainThreadTaskRunner = base::SequencedTaskRunner::GetCurrentDefault();
  }
  return self;
}

#pragma mark - CreditCardScannedImageDelegate

- (void)processOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
                         viewport:(CGRect)viewport {
  // Note: Current thread is an unknown background thread as this is a callback
  // from UIKit.

  // TODO(crbug.com/442869727): Currently we process buffers as fast as possible
  // as long as the camera is open. Excess buffers are discarded due to how
  // AVCaptureVideoDataOutput works, but we could consider further throttling
  // the capture rate. For example, it may be less valuable to analyze 10 almost
  // identical frames versus 10 frames over a longer timeframe which will have
  // slight changes as the camera and/or card move. However, there would be a
  // trade-off against overall latency for the user.

  if (!_textRecognitionRequest) {
    __weak __typeof(self) weakSelf = self;

    auto completionHandler = ^(VNRequest* request, NSError* error) {
      if (request.results.count != 0) {
        [weakSelf searchInRecognizedText:request.results];
      }
    };

    _textRecognitionRequest = [[VNRecognizeTextRequest alloc]
        initWithCompletionHandler:completionHandler];

    // Sets the region of interest of the request to the scanner viewport to
    // focus on the scan area. This improves performance by ignoring irrelevant
    // background text.
    //
    // TODO(crbug.com/442869727): Consider expanding the region of interest to
    // be slightly bigger than the viewport, to give some 'wiggle room' for the
    // user to not perfectly hold the card within it.
    _textRecognitionRequest.regionOfInterest = viewport;
    // Fast option doesn't recognise card number correctly.
    _textRecognitionRequest.recognitionLevel =
        VNRequestTextRecognitionLevelAccurate;
    // For time performance as we scan for numbers and date only.
    _textRecognitionRequest.usesLanguageCorrection = false;
  }

  CVPixelBufferRef pixelBuffer = CMSampleBufferGetImageBuffer(sampleBuffer);

  VNImageRequestHandler* handler =
      [[VNImageRequestHandler alloc] initWithCVPixelBuffer:pixelBuffer
                                                   options:@{}];

  NSError* requestError = nil;
  [handler performRequests:@[ _textRecognitionRequest ] error:&requestError];
}

#pragma mark - Helper Methods

- (void)setOrIncrementCount:(CandidateDictionary&)dictionary
                     forKey:(id<NSCopying>)key {
  NSNumber* newCount = @([dictionary._counts[key] intValue] + 1);
  dictionary._counts[key] = newCount;

  if (newCount > dictionary._maxCount) {
    dictionary._maxKey = key;
    dictionary._maxCount = newCount;
  }
}

- (KeyAndCount)mostCommonKey:(CandidateDictionary&)dictionary {
  return {
      ._key = dictionary._maxKey,
      ._count = [dictionary._maxCount intValue],
  };
}

// Searches in `recognizedText` for credit card number and expiration date, and
// subsequently informs the consumer if a sufficiently strong match has been
// found.
- (void)searchInRecognizedText:
    (NSArray<VNRecognizedTextObservation*>*)recognizedText {
  // Note: Current thread is an unknown background thread as this is a callback
  // from UIKit.

  for (VNRecognizedTextObservation* observation in recognizedText) {
    VNRecognizedText* candidate = [[observation topCandidates:1] firstObject];
    if (candidate == nil) {
      continue;
    }
    [self extractDataFromText:candidate.string];
  }

  // Check if we have now seen enough of a candidate card number to proceed.
  // Once we have seen the same number sufficient times, we can be confident it
  // was (probably) not a one-off misrecognition.
  KeyAndCount cardNumberAndCount = [self mostCommonKey:_candidateCardNumbers];
  if (cardNumberAndCount._count >= kMinConfidenceCount) {
    NSString* cardNumber = (NSString*)cardNumberAndCount._key;

    // Our minimum requirement is that we find a card number. Finding an expiry
    // date is an optional extra, so we don't require finding a minimum number
    // of dates.
    NSString* expirationMonth = nil;
    NSString* expirationYear = nil;
    NSDateComponents* expirationDate =
        (NSDateComponents*)[self mostCommonKey:_candidateExpirationDates]._key;
    if (expirationDate) {
      expirationMonth = [@([expirationDate month]) stringValue];
      expirationYear = [@([expirationDate year]) stringValue];
    }

    // Send the result to the main thread.
    __weak __typeof(self) weakSelf = self;
    _mainThreadTaskRunner->PostTask(FROM_HERE, base::BindOnce(^{
                                      [weakSelf
                                          handleFoundCreditCard:cardNumber
                                                expirationMonth:expirationMonth
                                                 expirationYear:expirationYear];
                                    }));
  }
}

// Attempts to extract either an expiry date or card number from the given text,
// and update the candidate maps accordingly.
- (void)extractDataFromText:(NSString*)text {
  NSDateComponents* date = ExtractExpirationDateFromText(text);
  if (date && [date month] != NSDateComponentUndefined &&
      [date year] != NSDateComponentUndefined) {
    [self setOrIncrementCount:_candidateExpirationDates forKey:date];
    return;
  }

  NSString* cardNumber = ExtractCreditCardNumber(text);
  if (cardNumber != nil) {
    [self setOrIncrementCount:_candidateCardNumbers forKey:cardNumber];
  }
}

- (void)handleFoundCreditCard:(NSString*)cardNumber
              expirationMonth:(NSString*)expirationMonth
               expirationYear:(NSString*)expirationYear {
  [_creditCardScannerConsumer setCreditCardNumber:cardNumber
                                  expirationMonth:expirationMonth
                                   expirationYear:expirationYear];
  // Avoid accidentally double-dispatching for any future frames.
  _creditCardScannerConsumer = nil;
}

@end
