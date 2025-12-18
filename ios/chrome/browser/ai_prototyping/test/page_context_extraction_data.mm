// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/test/page_context_extraction_data.h"

@implementation PageContextExtractionConfig

- (instancetype)initWithShouldStorePageContextLocally:(BOOL)shouldStore
                                            outputDir:(NSString*)outputDir {
  self = [super init];
  if (self) {
    _shouldStorePageContextLocally = shouldStore;
    _outputDir = outputDir;
  }
  return self;
}

#pragma mark - NSSecureCoding

+ (BOOL)supportsSecureCoding {
  return YES;
}

- (void)encodeWithCoder:(NSCoder*)coder {
  [coder encodeBool:self.shouldStorePageContextLocally
             forKey:@"shouldStorePageContextLocally"];
  [coder encodeBool:self.outputDir forKey:@"outputDir"];
}

- (instancetype)initWithCoder:(NSCoder*)coder {
  self = [super init];
  if (self) {
    _shouldStorePageContextLocally =
        [coder decodeBoolForKey:@"shouldStorePageContextLocally"];
    _outputDir = [coder decodeObjectOfClass:[NSString class]
                                     forKey:@"inputDir"];
  }
  return self;
}

@end

@implementation PageContextExtractionResult

- (instancetype)initWithPageContext:(NSString*)pageContext
                              error:(NSError*)error
                           filePath:(NSString*)filePath {
  self = [super init];
  if (self) {
    _pageContext = [pageContext copy];
    _error = [error copy];
    _filePath = [filePath copy];
  }
  return self;
}

#pragma mark - NSSecureCoding

+ (BOOL)supportsSecureCoding {
  return YES;
}

- (void)encodeWithCoder:(NSCoder*)coder {
  [coder encodeObject:self.pageContext forKey:@"pageContext"];
  [coder encodeObject:self.error forKey:@"error"];
  [coder encodeObject:self.filePath forKey:@"filePath"];
}

- (instancetype)initWithCoder:(NSCoder*)coder {
  self = [super init];
  if (self) {
    _pageContext = [coder decodeObjectOfClass:[NSString class]
                                       forKey:@"pageContext"];
    _error = [coder decodeObjectOfClass:[NSError class] forKey:@"error"];
    _filePath = [coder decodeObjectOfClass:[NSString class] forKey:@"filePath"];
  }
  return self;
}

@end
