// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/utils/nsobject_description_utils.h"

#import <objc/runtime.h>

NSString* CWVPropertiesDescription(id object) {
  NSMutableArray* properties = [NSMutableArray array];
  unsigned int outCount;
  objc_property_t* propertyList =
      class_copyPropertyList([object class], &outCount);
  for (unsigned int i = 0; i < outCount; i++) {
    objc_property_t property = propertyList[i];
    NSString* propertyName =
        [[NSString alloc] initWithCString:property_getName(property)
                                 encoding:NSUTF8StringEncoding];
    id propertyValue = [object valueForKey:propertyName];
    NSString* propertyDescription =
        [NSString stringWithFormat:@"%@: %@", propertyName, propertyValue];
    [properties addObject:propertyDescription];
  }
  free(propertyList);
  return [properties componentsJoinedByString:@"\n"];
}
