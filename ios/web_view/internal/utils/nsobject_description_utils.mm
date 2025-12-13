// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/utils/nsobject_description_utils.h"

#import <objc/runtime.h>

#import "base/containers/heap_array.h"
#import "base/memory/free_deleter.h"

namespace {

// Wraps a call to class_copyPropertyList(...) and returns the list of
// properties as an base::HeapArray<objc_property_t, ...>.
base::HeapArray<objc_property_t, base::FreeDeleter> GetProperties(Class cls) {
  unsigned int count = 0;
  objc_property_t* properties = class_copyPropertyList(cls, &count);

  // SAFETY: class_copyPropertyList(...) sets `count` to the number of
  // elements in the returned array.
  return UNSAFE_BUFFERS(
      base::HeapArray<objc_property_t, base::FreeDeleter>::FromOwningPointer(
          properties, count));
}

}  // namespace

NSString* CWVPropertiesDescription(id object) {
  NSMutableArray* properties = [NSMutableArray array];
  for (const objc_property_t& property : GetProperties([object class])) {
    NSString* propertyName =
        [[NSString alloc] initWithCString:property_getName(property)
                                 encoding:NSUTF8StringEncoding];
    id propertyValue = [object valueForKey:propertyName];
    NSString* propertyDescription =
        [NSString stringWithFormat:@"%@: %@", propertyName, propertyValue];
    [properties addObject:propertyDescription];
  }
  return [properties componentsJoinedByString:@"\n"];
}
