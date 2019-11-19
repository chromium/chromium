// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/credit_card_scanner/credit_card_scanner_image_processor.h"

#import <CoreMedia/CoreMedia.h>
#import <Vision/Vision.h>

#include "base/logging.h"
#import "ios/chrome/browser/ui/settings/credit_card_scanner/credit_card_consumer.h"
#import "ios/chrome/browser/ui/settings/credit_card_scanner/credit_card_scanner_string_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface CreditCardScannerImageProcessor ()

// An image analysis request that finds and recognizes text in an image.
@property(nonatomic, strong) VNRecognizeTextRequest* textRecognitionRequest;

// This property is for an interface which notfies the credit card consumer.
@property(nonatomic, weak) id<CreditCardConsumer> creditCardConsumer;

// The card number set after |textRecognitionRequest| from recognised text on
// the card.
@property(nonatomic, strong) NSString* cardNumber;

// The card expiration month set after |textRecognitionRequest| from recognised
// text on the card.
@property(nonatomic, strong) NSString* expirationMonth;

// The card expiration year set after |textRecognitionRequest| from recognised
// text on the card.
@property(nonatomic, strong) NSString* expirationYear;

@end

@implementation CreditCardScannerImageProcessor

#pragma mark - Lifecycle

- (instancetype)initWithConsumer:(id<CreditCardConsumer>)creditCardConsumer {
  self = [super init];
  if (self) {
    _creditCardConsumer = creditCardConsumer;
  }
  return self;
}

#pragma mark - CreditCardScannerImageDelegate

- (void)processOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
                         viewport:(CGRect)viewport {
  // Current thread is unknown background thread as is a callback from UIKit.
  DCHECK(!NSThread.isMainThread);
  if (!self.textRecognitionRequest) {
    __weak __typeof(self) weakSelf = self;

    auto completionHandler = ^(VNRequest* request, NSError* error) {
      if (request.results.count != 0) {
        [weakSelf searchInRecognizedText:request.results];
      }
    };

    self.textRecognitionRequest = [[VNRecognizeTextRequest alloc]
        initWithCompletionHandler:completionHandler];

    // Sets the region of interest of the request to the scanner viewport to
    // focus on the scan area. This improves performance by ignoring irrelevant
    // background text.
    self.textRecognitionRequest.regionOfInterest = viewport;
    // Fast option doesn't recognise card number correctly.
    self.textRecognitionRequest.recognitionLevel =
        VNRequestTextRecognitionLevelAccurate;
    // For time performance as we scan for numbers and date only.
    self.textRecognitionRequest.usesLanguageCorrection = false;
  }

  CVPixelBufferRef pixelBuffer = CMSampleBufferGetImageBuffer(sampleBuffer);
  DCHECK(pixelBuffer);

  NSMutableDictionary* options = [[NSMutableDictionary alloc] init];
  VNImageRequestHandler* handler =
      [[VNImageRequestHandler alloc] initWithCVPixelBuffer:pixelBuffer
                                                   options:options];

  NSError* requestError;
  [handler performRequests:@[ self.textRecognitionRequest ]
                     error:&requestError];

  // Error code 11 is unknown exception. It happens for some frames.
  DCHECK(!requestError || requestError.code == 11);
}

#pragma mark - Helper Methods

// Searches in |recognizedText| for credit card number and expiration date.
- (void)searchInRecognizedText:
    (NSArray<VNRecognizedTextObservation*>*)recognizedText {
  // Current thread is unknown background thread as is a callback from UIKit.
  DCHECK(!NSThread.isMainThread);
  NSUInteger maximumCandidates = 1;

  for (VNRecognizedTextObservation* observation in recognizedText) {
    VNRecognizedText* candidate =
        [[observation topCandidates:maximumCandidates] firstObject];
    if (candidate == nil) {
      continue;
    }
    [self extractDataFromText:candidate.string];
  }

  if (self.cardNumber) {
    // Send the result to the main thread.
    dispatch_async(dispatch_get_main_queue(), ^{
      [self.creditCardConsumer setCreditCardNumber:self.cardNumber
                                   expirationMonth:self.expirationMonth
                                    expirationYear:self.expirationYear];
    });
  }
}

// Checks the type of |text| to assign it to appropriate property.
- (void)extractDataFromText:(NSString*)text {
  if (!self.expirationMonth || !self.expirationYear) {
    NSDateComponents* components = ios::ExtractExpirationDateFromText(text);

    if (components) {
      self.expirationMonth = [@([components month]) stringValue];
      self.expirationYear = [@([components year]) stringValue];
    }
  }

  if (!self.cardNumber) {
    self.cardNumber = ios::ExtractCreditCardNumber(text);
  }
}

@end
