// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RLZ_MAC_LIB_RLZ_VALUE_STORE_MAC_H_
#define RLZ_MAC_LIB_RLZ_VALUE_STORE_MAC_H_

#include <stddef.h>
#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "rlz/lib/rlz_value_store.h"

@class NSDictionary;
@class NSMutableDictionary;

namespace rlz_lib {

// An implementation of RlzValueStore for mac. It stores information in a
// plist file in the user's Application Support folder.
class RlzValueStoreMac : public RlzValueStore {
 public:
  bool HasAccess(AccessType type) override;

  bool WritePingTime(Product product, int64_t time) override;
  bool ReadPingTime(Product product, int64_t* time) override;
  bool ClearPingTime(Product product) override;

  bool WriteAccessPointRlz(AccessPoint access_point,
                           const char* new_rlz) override;
  bool ReadAccessPointRlz(AccessPoint access_point,
                          char* rlz,
                          size_t rlz_size) override;
  bool ClearAccessPointRlz(AccessPoint access_point) override;
  bool UpdateExistingAccessPointRlz(const std::string& brand) override;

  bool AddProductEvent(Product product, const char* event_rlz) override;
  bool ReadProductEvents(Product product,
                         std::vector<std::string>* events) override;
  bool ClearProductEvent(Product product, const char* event_rlz) override;
  bool ClearAllProductEvents(Product product) override;

  bool AddStatefulEvent(Product product, const char* event_rlz) override;
  bool IsStatefulEvent(Product product, const char* event_rlz) override;
  bool ClearAllStatefulEvents(Product product) override;

  void CollectGarbage() override;

 private:
  // |dict| is the dictionary that backs all data. plist_path is the name of the
  // plist file, used solely for implementing HasAccess().
  RlzValueStoreMac(NSMutableDictionary* dict, NSString* plist_path);
  ~RlzValueStoreMac() override;
  friend class ScopedRlzValueStoreLock;

  // Returns the backing dictionary that should be written to disk.
  NSDictionary* dictionary();

  // Returns the dictionary to which all data should be written. Usually, this
  // is just |dictionary()|, but if supplementary branding is used, it's a
  // subdirectory at key "brand_<supplementary branding code>".
  // Note that windows stores data at
  //    rlz/name (e.g. "pingtime")/supplementalbranding/productcode
  // Mac on the other hand does
  //    supplementalbranding/productcode/pingtime.
  NSMutableDictionary* WorkingDict();

  // Returns the subdirectory of |WorkingDict()| used to store data for
  // product p.
  NSMutableDictionary* ProductDict(Product p);

  base::scoped_nsobject<NSMutableDictionary> dict_;
  base::scoped_nsobject<NSString> plist_path_;

  DISALLOW_COPY_AND_ASSIGN(RlzValueStoreMac);
};

}  // namespace rlz_lib

#endif  // RLZ_MAC_LIB_RLZ_VALUE_STORE_MAC_H_
