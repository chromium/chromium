// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NAVIGATION_SERIALIZABLE_USER_DATA_MANAGER_IMPL_H_
#define IOS_WEB_NAVIGATION_SERIALIZABLE_USER_DATA_MANAGER_IMPL_H_

#include "base/macros.h"
#import "ios/web/public/session/serializable_user_data_manager.h"

namespace web {

class SerializableUserDataImpl : public SerializableUserData {
 public:
  SerializableUserDataImpl();
  ~SerializableUserDataImpl() override;

  // Constructor taking the NSDictionary holding the serializable data.
  explicit SerializableUserDataImpl(
      NSDictionary<NSString*, id<NSCoding>>* data);

  // SerializableUserData:
  void Encode(NSCoder* coder) override;
  void Decode(NSCoder* coder) override;

  // Returns the serializable data.
  NSDictionary<NSString*, id<NSCoding>>* data() { return data_; }

 private:
  // The dictionary passed on initialization.  After calling Decode(), this will
  // contain the data that is decoded from the NSCoder.
  NSDictionary<NSString*, id<NSCoding>>* data_;

  DISALLOW_COPY_AND_ASSIGN(SerializableUserDataImpl);
};

class SerializableUserDataManagerImpl : public SerializableUserDataManager {
 public:
  SerializableUserDataManagerImpl();
  ~SerializableUserDataManagerImpl();

  // SerializableUserDataManager:
  void AddSerializableData(id<NSCoding> data, NSString* key) override;
  id<NSCoding> GetValueForSerializationKey(NSString* key) override;
  std::unique_ptr<SerializableUserData> CreateSerializableUserData()
      const override;
  void AddSerializableUserData(SerializableUserData* data) override;

 private:
  // The dictionary that stores serializable user data.
  NSMutableDictionary<NSString*, id<NSCoding>>* data_;

  DISALLOW_COPY_AND_ASSIGN(SerializableUserDataManagerImpl);
};

}  // namespace web

#endif  // IOS_WEB_NAVIGATION_SERIALIZABLE_USER_DATA_MANAGER_IMPL_H_
