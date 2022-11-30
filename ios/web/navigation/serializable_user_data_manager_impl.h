// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NAVIGATION_SERIALIZABLE_USER_DATA_MANAGER_IMPL_H_
#define IOS_WEB_NAVIGATION_SERIALIZABLE_USER_DATA_MANAGER_IMPL_H_

#import "ios/web/public/session/serializable_user_data_manager.h"

namespace web {

class SerializableUserDataManagerImpl : public SerializableUserDataManager {
 public:
  SerializableUserDataManagerImpl();

  SerializableUserDataManagerImpl(const SerializableUserDataManagerImpl&) =
      delete;
  SerializableUserDataManagerImpl& operator=(
      const SerializableUserDataManagerImpl&) = delete;

  ~SerializableUserDataManagerImpl();

  // SerializableUserDataManager:
  void AddSerializableData(id<NSCoding> data, NSString* key) override;
  id<NSCoding> GetValueForSerializationKey(NSString* key) override;
  CRWSessionUserData* GetUserDataForSession() const override;
  void SetUserDataFromSession(CRWSessionUserData* data) override;

 private:
  // The object storing the user data.
  __strong CRWSessionUserData* data_;
};

}  // namespace web

#endif  // IOS_WEB_NAVIGATION_SERIALIZABLE_USER_DATA_MANAGER_IMPL_H_
