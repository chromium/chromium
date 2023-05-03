// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "rlz/mac/lib/rlz_value_store_mac.h"

#import <Foundation/Foundation.h>

#include <tuple>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/mac/foundation_util.h"
#include "base/notreached.h"
#include "base/strings/sys_string_conversions.h"
#include "rlz/lib/assert.h"
#include "rlz/lib/lib_values.h"
#include "rlz/lib/recursive_cross_process_lock_posix.h"
#include "rlz/lib/supplementary_branding.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::mac::ObjCCast;

namespace rlz_lib {

// These are written to disk and should not be changed.
NSString* const kPingTimeKey = @"pingTime";
NSString* const kAccessPointKey = @"accessPoints";
NSString* const kProductEventKey = @"productEvents";
NSString* const kStatefulEventKey = @"statefulEvents";

namespace {

NSString* GetNSProductName(Product product) {
  return base::SysUTF8ToNSString(GetProductName(product));
}

NSString* GetNSAccessPointName(AccessPoint p) {
  return base::SysUTF8ToNSString(GetAccessPointName(p));
}

// Retrieves a subdictionary in |p| for key |k|, creating it if necessary.
// If the dictionary contains an object for |k| that is not a mutable
// dictionary, that object is replaced with an empty mutable dictionary.
NSMutableDictionary* GetOrCreateDict(NSMutableDictionary* p, NSString* k) {
  NSMutableDictionary* d = ObjCCast<NSMutableDictionary>(p[k]);
  if (!d) {
    d = [NSMutableDictionary dictionary];
    p[k] = d;
  }
  return d;
}

}  // namespace

RlzValueStoreMac::RlzValueStoreMac(NSMutableDictionary* dict,
                                   NSString* plist_path)
    : dict_(dict), plist_path_(plist_path) {}

RlzValueStoreMac::~RlzValueStoreMac() = default;

bool RlzValueStoreMac::HasAccess(AccessType type) {
  switch (type) {
    case kReadAccess:
      return [NSFileManager.defaultManager isReadableFileAtPath:plist_path_];
    case kWriteAccess:
      return [NSFileManager.defaultManager isWritableFileAtPath:plist_path_];
  }
}

bool RlzValueStoreMac::WritePingTime(Product product, int64_t time) {
  ProductDict(product)[kPingTimeKey] = @(time);
  return true;
}

bool RlzValueStoreMac::ReadPingTime(Product product, int64_t* time) {
  if (NSNumber* n = ObjCCast<NSNumber>(ProductDict(product)[kPingTimeKey])) {
    *time = n.longLongValue;
    return true;
  }
  return false;
}

bool RlzValueStoreMac::ClearPingTime(Product product) {
  [ProductDict(product) removeObjectForKey:kPingTimeKey];
  return true;
}

bool RlzValueStoreMac::WriteAccessPointRlz(AccessPoint access_point,
                                           const char* new_rlz) {
  NSMutableDictionary* d = GetOrCreateDict(WorkingDict(), kAccessPointKey);
  d[GetNSAccessPointName(access_point)] = base::SysUTF8ToNSString(new_rlz);
  return true;
}

bool RlzValueStoreMac::ReadAccessPointRlz(AccessPoint access_point,
                                          char* rlz,
                                          size_t rlz_size) {
  // Reading a non-existent access point counts as success.
  if (NSDictionary* d =
          ObjCCast<NSDictionary>(WorkingDict()[kAccessPointKey])) {
    NSString* val = ObjCCast<NSString>(d[GetNSAccessPointName(access_point)]);
    if (!val) {
      if (rlz_size > 0) {
        rlz[0] = '\0';
      }
      return true;
    }

    std::string s = base::SysNSStringToUTF8(val);
    if (s.size() >= rlz_size) {
      rlz[0] = 0;
      ASSERT_STRING("GetAccessPointRlz: Insufficient buffer size");
      return false;
    }
    strncpy(rlz, s.c_str(), rlz_size);
    return true;
  }
  if (rlz_size > 0) {
    rlz[0] = '\0';
  }
  return true;
}

bool RlzValueStoreMac::ClearAccessPointRlz(AccessPoint access_point) {
  if (NSMutableDictionary* d =
          ObjCCast<NSMutableDictionary>(WorkingDict()[kAccessPointKey])) {
    [d removeObjectForKey:GetNSAccessPointName(access_point)];
  }
  return true;
}

bool RlzValueStoreMac::UpdateExistingAccessPointRlz(const std::string& brand) {
  return false;
}

bool RlzValueStoreMac::AddProductEvent(Product product, const char* event_rlz) {
  GetOrCreateDict(ProductDict(product),
                  kProductEventKey)[base::SysUTF8ToNSString(event_rlz)] = @YES;
  return true;
}

bool RlzValueStoreMac::ReadProductEvents(Product product,
                                         std::vector<std::string>* events) {
  if (NSDictionary* d =
          ObjCCast<NSDictionary>(ProductDict(product)[kProductEventKey])) {
    for (NSString* s in d) {
      events->push_back(base::SysNSStringToUTF8(s));
    }
    return true;
  }
  return true;
}

bool RlzValueStoreMac::ClearProductEvent(Product product,
                                         const char* event_rlz) {
  if (NSMutableDictionary* d = ObjCCast<NSMutableDictionary>(
          ProductDict(product)[kProductEventKey])) {
    [d removeObjectForKey:base::SysUTF8ToNSString(event_rlz)];
    return true;
  }
  return false;
}

bool RlzValueStoreMac::ClearAllProductEvents(Product product) {
  [ProductDict(product) removeObjectForKey:kProductEventKey];
  return true;
}

bool RlzValueStoreMac::AddStatefulEvent(Product product,
                                        const char* event_rlz) {
  GetOrCreateDict(ProductDict(product),
                  kStatefulEventKey)[base::SysUTF8ToNSString(event_rlz)] = @YES;
  return true;
}

bool RlzValueStoreMac::IsStatefulEvent(Product product, const char* event_rlz) {
  if (NSDictionary* d =
          ObjCCast<NSDictionary>(ProductDict(product)[kStatefulEventKey])) {
    return d[base::SysUTF8ToNSString(event_rlz)] != nil;
  }
  return false;
}

bool RlzValueStoreMac::ClearAllStatefulEvents(Product product) {
  [ProductDict(product) removeObjectForKey:kStatefulEventKey];
  return true;
}

void RlzValueStoreMac::CollectGarbage() {
  NOTIMPLEMENTED();
}

NSDictionary* RlzValueStoreMac::dictionary() {
  return dict_;
}

NSMutableDictionary* RlzValueStoreMac::WorkingDict() {
  std::string brand(SupplementaryBranding::GetBrand());
  if (brand.empty()) {
    return dict_;
  }

  NSString* brand_ns =
      [@"brand_" stringByAppendingString:base::SysUTF8ToNSString(brand)];

  return GetOrCreateDict(dict_, brand_ns);
}

NSMutableDictionary* RlzValueStoreMac::ProductDict(Product p) {
  return GetOrCreateDict(WorkingDict(), GetNSProductName(p));
}

namespace {

RecursiveCrossProcessLock g_recursive_lock =
    RECURSIVE_CROSS_PROCESS_LOCK_INITIALIZER;

// This is set during test execution, to write RLZ files into a temporary
// directory instead of the user's Application Support folder.
NSString* __strong g_test_folder;

// RlzValueStoreMac keeps its data in memory and only writes it to disk when
// ScopedRlzValueStoreLock goes out of scope. Hence, if several
// ScopedRlzValueStoreLocks are nested, they all need to use the same store
// object.

// This counts the nesting depth.
int g_lock_depth = 0;

// This is the store object that might be shared. Only set if g_lock_depth > 0.
RlzValueStoreMac* g_store_object = nullptr;

NSString* CreateRlzDirectory() {
  NSArray* paths = NSSearchPathForDirectoriesInDomains(
      NSApplicationSupportDirectory, NSUserDomainMask, /*expandTilde=*/YES);
  NSString* folder = nil;
  if (paths.count > 0) {
    folder = ObjCCast<NSString>(paths[0]);
  }
  if (!folder) {
    folder = [@"~/Library/Application Support" stringByStandardizingPath];
  }
  folder = [folder stringByAppendingPathComponent:@"Google/RLZ"];

  if (g_test_folder) {
    folder = [g_test_folder stringByAppendingPathComponent:folder];
  }

  [NSFileManager.defaultManager createDirectoryAtPath:folder
                          withIntermediateDirectories:YES
                                           attributes:nil
                                                error:nil];
  return folder;
}

// Returns the path of the rlz plist store, also creates the parent directory
// path if it doesn't exist.
NSString* RlzPlistFilename() {
  NSString* const kRlzFile = @"RlzStore.plist";
  return [CreateRlzDirectory() stringByAppendingPathComponent:kRlzFile];
}

// Returns the path of the rlz lock file, also creates the parent directory
// path if it doesn't exist.
NSString* RlzLockFilename() {
  NSString* const kRlzLockfile = @"flockfile";
  return [CreateRlzDirectory() stringByAppendingPathComponent:kRlzLockfile];
}

}  // namespace

ScopedRlzValueStoreLock::ScopedRlzValueStoreLock() {
  bool got_distributed_lock = g_recursive_lock.TryGetCrossProcessLock(
      base::mac::NSStringToFilePath(RlzLockFilename()));
  // At this point, we hold the in-process lock, no matter the value of
  // |got_distributed_lock|.

  ++g_lock_depth;

  if (!got_distributed_lock) {
    // Give up. |store_| isn't set, which signals to callers that acquiring
    // the lock failed. |g_recursive_lock| will be released by the
    // destructor.
    CHECK(!g_store_object);
    return;
  }

  if (g_lock_depth > 1) {
    // Reuse the already existing store object. Note that it can be NULL when
    // lock acquisition succeeded but the rlz data file couldn't be read.
    store_.reset(g_store_object);
    return;
  }

  CHECK(!g_store_object);

  NSString* plist = RlzPlistFilename();

  // Create an empty file if none exists yet.
  if (![NSFileManager.defaultManager fileExistsAtPath:plist isDirectory:nil]) {
    [[NSDictionary dictionary] writeToFile:plist atomically:YES];
  }

  NSMutableDictionary* dict =
      [NSMutableDictionary dictionaryWithContentsOfFile:plist];
  VERIFY(dict);

  if (dict) {
    store_.reset(new RlzValueStoreMac(dict, plist));
    g_store_object = (RlzValueStoreMac*)store_.get();
  }
}

ScopedRlzValueStoreLock::~ScopedRlzValueStoreLock() {
  --g_lock_depth;
  CHECK(g_lock_depth >= 0);

  if (g_lock_depth > 0) {
    // Other locks are still using store_, don't free it yet.
    std::ignore = store_.release();
    return;
  }

  if (store_.get()) {
    g_store_object = nullptr;

    NSDictionary* dict =
        static_cast<RlzValueStoreMac*>(store_.get())->dictionary();
    VERIFY([dict writeToFile:RlzPlistFilename() atomically:YES]);
  }

  // Check that "store_ set" => "file_lock acquired". The converse isn't true,
  // for example if the rlz data file can't be read.
  if (store_.get()) {
    CHECK(g_recursive_lock.file_lock_ != -1);
  }
  if (g_recursive_lock.file_lock_ == -1) {
    CHECK(!store_.get());
  }

  g_recursive_lock.ReleaseLock();
}

RlzValueStore* ScopedRlzValueStoreLock::GetStore() {
  return store_.get();
}

namespace testing {

void SetRlzStoreDirectory(const base::FilePath& directory) {
  @autoreleasepool {
    if (directory.empty()) {
      g_test_folder = nil;
    } else {
      g_test_folder = base::mac::FilePathToNSString(directory);
    }
  }
}

std::string RlzStoreFilenameStr() {
  @autoreleasepool {
    return std::string(RlzPlistFilename().fileSystemRepresentation);
  }
}

}  // namespace testing

}  // namespace rlz_lib
