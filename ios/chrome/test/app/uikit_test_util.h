// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_APP_UIKIT_TEST_UTIL_H_
#define IOS_CHROME_TEST_APP_UIKIT_TEST_UTIL_H_

#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "base/location.h"
#import "base/strings/sys_string_conversions.h"
#import "testing/gtest/include/gtest/gtest.h"

namespace chrome_test_util {

// Returns the UIWindowScene found from the list of connected scenes. Used only
// for testing.
UIWindowScene* GetAnyWindowScene();

// Recursively searches `view` and its subviews for all views for which
// `predicate` returns YES. Both `view` and `predicate` must not be nil. Checks
// that all matching views are of type `T`.
// `found_views` must be nil except for recursive calls.
template <typename T = UIView>
NSMutableArray<T*>* FindViewsWithPredicate(
    UIView* view,
    BOOL (^predicate)(UIView*),
    NSMutableArray<T*>* found_views = nil) {
  EXPECT_NE(view, nil);
  EXPECT_NE(predicate, nil);
  if (!found_views) {
    found_views = [NSMutableArray array];
  }
  if (predicate(view)) {
    T* found = base::apple::ObjCCast<T>(view);
    EXPECT_NE(found, nil)
        << base::SysNSStringToUTF8(view.description) << " should be of type "
        << base::SysNSStringToUTF8(NSStringFromClass([T class]))
        << "* but is of type "
        << base::SysNSStringToUTF8(NSStringFromClass([view class])) << "*";
    [found_views addObject:found];
  }
  for (UIView* subview in view.subviews) {
    FindViewsWithPredicate<T>(subview, predicate, found_views);
  }
  return found_views;
}

// Recursively searches `view` and its subviews for a view for which
// `predicate` returns YES. Returns the first matching view, or nil if not
// found. Both `view` and `predicate` must not be nil. Checks that all matching
// views are of type `T`. If `expect_uniqueness` is true, expects at most one
// view to match `predicate` (reporting failures at `location`).
template <typename T = UIView>
T* FindViewWithPredicate(UIView* view,
                         BOOL (^predicate)(UIView*),
                         bool expect_uniqueness = true,
                         const base::Location& location = FROM_HERE) {
  NSArray<T*>* found_views = FindViewsWithPredicate<T>(view, predicate);
  if (expect_uniqueness) {
    EXPECT_LE(found_views.count, 1u)
        << "View " << base::SysNSStringToUTF8(view.description)
        << " was expected to have at most 1 matching subview of type "
        << base::SysNSStringToUTF8(NSStringFromClass([T class])) << ", but "
        << found_views.count << " were found.\n"
        << location.ToString();
  }
  return found_views.firstObject;
}

// Recursively searches `view` and its subviews for a view with the given
// `accessibility_id`. Returns the first matching view, or nil if not found.
// Both `view` and `accessibility_id` must not be nil. Checks that all matching
// views are of type `T`. If `expect_uniqueness` is true, expects at most one
// matching view (reporting failures at `location`).
template <typename T = UIView>
T* FindViewById(UIView* view,
                NSString* accessibility_id,
                bool expect_uniqueness = true,
                const base::Location& location = FROM_HERE) {
  EXPECT_NE(view, nil);
  EXPECT_NE(accessibility_id, nil);
  return FindViewWithPredicate<T>(
      view,
      ^BOOL(UIView* candidate) {
        return [candidate.accessibilityIdentifier
            isEqualToString:accessibility_id];
      },
      expect_uniqueness, location);
}

// Recursively searches `view` and its subviews for a view that is an instance
// of `T` (or a subclass). Returns the first matching view, or nil if not
// found. `view` must not be nil. If `expect_uniqueness` is true, expects at
// most one matching view (reporting failures at `location`).
template <typename T>
T* FindViewByClass(UIView* view,
                   bool expect_uniqueness = true,
                   const base::Location& location = FROM_HERE) {
  return FindViewWithPredicate<T>(
      view,
      ^BOOL(UIView* candidate) {
        return [candidate isKindOfClass:[T class]];
      },
      expect_uniqueness, location);
}

// Recursively searches `view` and its subviews for all views that are instances
// of `T` (or a subclass). `view` must not be nil.
template <typename T>
NSArray<T*>* FindViewsByClass(UIView* view) {
  return FindViewsWithPredicate<T>(view, ^BOOL(UIView* candidate) {
    return [candidate isKindOfClass:[T class]];
  });
}

// Expects `view` and its subviews to contain `count` views that are instances
// of `T` (or a subclass). `view` must not be nil.
template <typename T = UIView>
void ExpectSubviewCount(UIView* view,
                        size_t count,
                        const base::Location& location = FROM_HERE) {
  size_t actual_count = FindViewsByClass<T>(view).count;
  EXPECT_EQ(actual_count, count)
      << "View " << base::SysNSStringToUTF8(view.description)
      << " was expected to have exactly " << count << " subviews of type "
      << base::SysNSStringToUTF8(NSStringFromClass([T class])) << ", but "
      << actual_count << " were found.\n"
      << location.ToString();
}

// Expects `view` and its subviews to contain no views that are instances of `T`
// (or a subclass). `view` must not be nil.
template <typename T = UIView>
void ExpectNoSubview(UIView* view, const base::Location& location = FROM_HERE) {
  ExpectSubviewCount<T>(view, 0, location);
}

// Expects `view` and its subviews to contain a single view that is an instance
// of `T` (or a subclass). `view` must not be nil.
template <typename T = UIView>
void ExpectUniqueSubview(UIView* view,
                         const base::Location& location = FROM_HERE) {
  ExpectSubviewCount<T>(view, 1, location);
}

// Recursively searches `view` and its subviews for a UILabel with the given
// `text`. Returns the first matching label, or nil if not found. Both `view`
// and `text` must not be nil. If `expect_uniqueness` is true, expects at most
// one matching label (reporting failures at `location`).
UILabel* FindLabelWithText(UIView* view,
                           NSString* text,
                           bool expect_uniqueness = true,
                           const base::Location& location = FROM_HERE);

}  // namespace chrome_test_util

#endif  // IOS_CHROME_TEST_APP_UIKIT_TEST_UTIL_H_
