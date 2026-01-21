// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/test/page_context_extraction_data.h"

@implementation PageContextExtractionConfig

- (instancetype)initWithShouldStorePageContextLocally:(BOOL)shouldStore
                                   shouldUploadToMQLS:(BOOL)shouldUpload
                                            outputDir:(NSString*)outputDir
                                           modelQuery:(NSString*)modelQuery
                                       mqlsLoggingTag:
                                           (NSString*)mqlsLoggingTag {
  self = [super init];
  if (self) {
    _shouldStorePageContextLocally = shouldStore;
    _shouldUploadToMQLS = shouldUpload;
    _outputDir = outputDir;
    _modelQuery = modelQuery;
    _mqlsLoggingTag = mqlsLoggingTag;
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
  [coder encodeBool:self.shouldUploadToMQLS forKey:@"shouldUploadToMQLS"];
  [coder encodeObject:self.outputDir forKey:@"outputDir"];
  [coder encodeObject:self.modelQuery forKey:@"modelQuery"];
  [coder encodeObject:self.mqlsLoggingTag forKey:@"mqlsLoggingTag"];
}

- (instancetype)initWithCoder:(NSCoder*)coder {
  self = [super init];
  if (self) {
    _shouldStorePageContextLocally =
        [coder decodeBoolForKey:@"shouldStorePageContextLocally"];
    _shouldUploadToMQLS = [coder decodeBoolForKey:@"shouldUploadToMQLS"];
    _outputDir = [coder decodeObjectOfClass:[NSString class]
                                     forKey:@"outputDir"];
    _modelQuery = [coder decodeObjectOfClass:[NSString class]
                                      forKey:@"modelQuery"];
    _mqlsLoggingTag = [coder decodeObjectOfClass:[NSString class]
                                          forKey:@"mqlsLoggingTag"];
  }
  return self;
}

@end

@implementation PageContextExtractionResult

- (instancetype)initWithPageContext:(NSString*)pageContext
                       wrapperError:(NSError*)wrapperError
                         storeError:(NSError*)storeError
                          mqlsError:(NSError*)mqlsError
                           filePath:(NSString*)filePath {
  self = [super init];
  if (self) {
    _pageContext = [pageContext copy];
    _wrapperError = [wrapperError copy];
    _storeError = [storeError copy];
    _mqlsError = [mqlsError copy];
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
  [coder encodeObject:self.wrapperError forKey:@"wrapperError"];
  [coder encodeObject:self.storeError forKey:@"storeError"];
  [coder encodeObject:self.mqlsError forKey:@"mqlsError"];
  [coder encodeObject:self.filePath forKey:@"filePath"];
}

- (instancetype)initWithCoder:(NSCoder*)coder {
  self = [super init];
  if (self) {
    _pageContext = [coder decodeObjectOfClass:[NSString class]
                                       forKey:@"pageContext"];
    _wrapperError = [coder decodeObjectOfClass:[NSError class]
                                        forKey:@"wrapperError"];
    _storeError = [coder decodeObjectOfClass:[NSError class]
                                      forKey:@"storeError"];
    _mqlsError = [coder decodeObjectOfClass:[NSError class]
                                     forKey:@"mqlsError"];
    _filePath = [coder decodeObjectOfClass:[NSString class] forKey:@"filePath"];
  }
  return self;
}

@end
